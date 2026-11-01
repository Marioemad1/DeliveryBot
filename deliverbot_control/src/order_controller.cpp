#include "rclcpp/rclcpp.hpp" //importing the ros2 lib if it shows a error open vs from the terminal
#include "rclcpp_action/rclcpp_action.hpp" //importing the action lib
#include "deliverbot_msgs/action/order_request.hpp"
#include "deliverbot_msgs/msg/cordinate.hpp"
#include "std_msgs/msg/bool.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using Goal = geometry_msgs::msg::PoseStamped;
using BOOL = std_msgs::msg::Bool;
using Cordinate = deliverbot_msgs::msg::Cordinate;
using namespace std::placeholders ;
using OrderRequest = deliverbot_msgs::action::OrderRequest;
using OrderControllerGoalHandler = rclcpp_action::ServerGoalHandle<OrderRequest>;

class OrderController : public rclcpp::Node //create a class and inharet from the rclcpp
{
    public: //puplic
        OrderController() : Node("order_controller") 
        {

            

            this->cb_group_ = this->create_callback_group(
                rclcpp::CallbackGroupType::Reentrant);

            //assign the v and the methods
            this->order_controller_ = rclcpp_action::create_server<OrderRequest>(
                this,
                "order_request",
                std::bind(&OrderController::goal_callback,  this, _1 , _2 ),
                std::bind(&OrderController::cancel_callback,this, _1 ),
                std::bind(&OrderController::handler_callback,this, _1),
                rcl_action_server_get_default_options(),cb_group_);

            this->places_ = {"red","blue","yellow","green"};

            this->goal_publisher_ = this->create_publisher<Cordinate>("create_goal",10);
            this->nav2_done_sub_ = this->create_subscription<BOOL>("nav2_done",10,
            std::bind(&OrderController::nav2_done_callback,this,_1));

            this->currecnt_pose_sub_ = this->create_subscription<Goal>("current_pose",10,
            std::bind(&OrderController::currecnt_pose_callback,this,_1));

            this->door_feedback_sub_ = this->create_subscription<BOOL>("door_feedback",10,
            std::bind(&OrderController::door_callback,this,_1));

            this->glob_nav2_done_ =false;
            this->glob_door_state_=false;

            this->set_goal(&this->red_,   -12.0  ,3.0  ,0.0);
            this->set_goal(&this->blue_,   8.0   ,3.0  ,0.0);
            this->set_goal(&this->yellow_, 8.0   ,-3.0 ,0.0);
            this->set_goal(&this->green_, -12.0  ,-3.0 ,0.0 );

            this->glob_current_pose_.pose.position.x= 0 ;
            this->glob_current_pose_.pose.position.y= 0 ;

            

            RCLCPP_INFO(this->get_logger(),"order controller is running");
            this-> goal_queue_thread_ = std::thread(&OrderController::execute_goal_thread,this); //start the thread at the end 
        }

        ~OrderController()
        {
            goal_queue_thread_.join();
        }
    private:
    //define the v of the class and the private methods
        
        //goal callback function
        rclcpp_action::GoalResponse goal_callback(
            const rclcpp_action::GoalUUID &uuid, std::shared_ptr<const OrderRequest::Goal> goal)
        {
            (void)uuid;
            OrderRequest::Goal target;

            target.start = goal->start;
            target.end = goal->end;

            if (!target.start.empty() && !target.end.empty())
            {
                target.start = convert_to_lowercase(target.start);
                target.end = convert_to_lowercase(target.end);

                if (is_it_invector(this->places_,target.start) && is_it_invector(this->places_,target.end))
                {
                    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE ;
                }

                
            }
            
            return rclcpp_action::GoalResponse::REJECT;
            
        }

        //cancel responce function 
        rclcpp_action::CancelResponse cancel_callback(
            const std::shared_ptr<OrderControllerGoalHandler> goal_handler )
        {
            (void)goal_handler;
            return rclcpp_action::CancelResponse::ACCEPT;
        }

        //goal_handler function 

        void handler_callback(
            const std::shared_ptr<OrderControllerGoalHandler> goal_handler )
        {
            std::lock_guard<std::mutex> lock(mutex_);
            goals_qeue_.push(goal_handler);
            RCLCPP_INFO(this->get_logger(),"Add a goal to the queue");
            RCLCPP_INFO(this->get_logger(),"the queue size: %d",(int)goals_qeue_.size());

        }

        //goal execute like py (optional)
        void execute_goal_thread ()
        {
            rclcpp::Rate loop_rate(1000.0);
            bool did_you_go_home =true;
            while (rclcpp::ok())
            {
                std::shared_ptr<OrderControllerGoalHandler> next_goal;
                {
                    std::lock_guard<std::mutex> lock(this->mutex_);
                    if (this->goals_qeue_.size()>0)
                    {
                        next_goal = this->goals_qeue_.front();
                        this->goals_qeue_.pop();
                        did_you_go_home = false;
                    }
                    else if (this->goals_qeue_.size() <= 0 && !did_you_go_home )
                    {
                        Cordinate home;
                        this->set_goal(&home,0.0 ,0.0 ,0.0);
                        this->goal_publisher_->publish(home);
                        did_you_go_home = true;
                    }
                    
                }
                if (next_goal)
                {
                    this->executegoal(next_goal);
                }
                loop_rate.sleep(); 
            }
        
        }

        void executegoal(
            const std::shared_ptr<OrderControllerGoalHandler> goal_handler)
        {
            std::string start = goal_handler->get_goal()->start;
            std::string end   = goal_handler->get_goal()->end;

            Cordinate start_cor = who_iam(start);
            Cordinate end_cor = who_iam(end);

            auto result   = std::make_shared<OrderRequest::Result>();
            auto feedback = std::make_shared<OrderRequest::Feedback>();

            bool order_completed = false;

            int stage = 0;
            rclcpp::Rate loop_rate(1000.0);
            while (rclcpp::ok())
            {
                if (goal_handler->is_canceling())
                {
                    RCLCPP_WARN(this->get_logger(),"Canceling the goal");

                    result->msg = where_iam(this->glob_current_pose_);
                    goal_handler->canceled(result);

                    return;
                }

                if (order_completed)
                {
                    RCLCPP_INFO(this->get_logger(),"Order is completed");

                    result->msg = where_iam(this->glob_current_pose_);
                    goal_handler->succeed(result);

                    return;
                }
                

                if (stage == 0)
                {   
                    this->glob_nav2_done_  = false;
                    this->glob_door_state_ = false;
                    stage = 1;
                    this->goal_publisher_->publish(start_cor);
                
                    continue;
                }
                else if (stage == 1)
                {   
                    static bool door_opened = false;   // remembers if door was opened
                
                    if (this->glob_nav2_done_)
                    {
                        if (this->glob_door_state_ == true) {
                            door_opened = true;        // user opened the door
                        }
                    
                        // only leave when door was opened AND is now closed again
                        if (door_opened && this->glob_door_state_ == false)
                        {
                            stage = 2;
                            this->glob_door_state_ = false;
                            this->glob_nav2_done_ = false;
                            door_opened = false;  
                            this->goal_publisher_->publish(end_cor);
                            continue;
                        }
                    }
                    continue;
                }
                else if (stage == 2)
                {
                    static bool door_opened = false;
                
                    if (this->glob_nav2_done_)
                    {
                        if (this->glob_door_state_ == true) {
                            door_opened = true;
                        }
                    
                        if (door_opened && this->glob_door_state_ == false)
                        {
                            this->glob_door_state_ = false;
                            this->glob_nav2_done_ = false;
                            door_opened = false;
                            order_completed = true;
                        }
                    }
                    continue;
                }
                else
                {
                    return;
                }
                   
            }

        }

        void currecnt_pose_callback(Goal pose)
        {
            this->glob_current_pose_ =pose;
        }

        void nav2_done_callback(BOOL msg)
        {
            this->glob_nav2_done_ = msg.data;
        }

        void door_callback(BOOL state)
        {
            this->glob_door_state_ = state.data;
        }

        std::string convert_to_lowercase(const std::string str)
        {
            std::string str_lower = "";
            for (char letter: str)
            {
                str_lower += std::tolower(letter);
            }
            
            return str_lower;
        }

        bool is_it_invector(std::vector<std::string> vector, std::string target)
        {
            if (std::find(vector.begin(),vector.end(),target) != vector.end())
            {
                return true;
            }
            
            return false;
        }

        void set_goal(Cordinate* goal,float x ,float y, float z)
        {
            goal->x = x;
            goal->y = y;
            goal->theta = z;
        }

        std::string where_iam(Goal current_pose)
        {
            auto x = current_pose.pose.position.x;
            auto y = current_pose.pose.position.y;

            if ( x > 0 && y > 0)
            {
                return "blue";
            }

            else if (x > 0 && y < 0)
            {
                return "yellow";
            }

            else if (x < 0 && y > 0)
            {
                return "red";
            }

            else if (x < 0 && y < 0)
            {
                return "green";
            }
            else return "home";
            
            
        }

        Cordinate who_iam(std:: string target)
        {
            if (target == "red")return this->red_;
            else if (target == "blue")return this->blue_;
            else if (target == "yellow")return this->yellow_;
            else if (target == "green") return this->green_;

            Cordinate dummy;
            set_goal(&dummy, 0.0, 0.0, 0.0);
            return dummy;
        }
    
    rclcpp::CallbackGroup::SharedPtr cb_group_;

    rclcpp_action::Server<OrderRequest>::SharedPtr order_controller_;
    std::vector<std::string> places_;
    std::queue<std::shared_ptr<OrderControllerGoalHandler>> goals_qeue_;
    std::mutex mutex_;
    std::thread goal_queue_thread_;

    rclcpp::Publisher<Cordinate>::SharedPtr goal_publisher_;
    rclcpp::Subscription<BOOL>::SharedPtr nav2_done_sub_;
    rclcpp::Subscription<Goal>::SharedPtr currecnt_pose_sub_;

    rclcpp::Subscription<BOOL>::SharedPtr door_feedback_sub_;

    

    Cordinate red_;
    Cordinate blue_;
    Cordinate yellow_;
    Cordinate green_;

    bool glob_nav2_done_;
    bool glob_door_state_;
    Goal glob_current_pose_;
};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);
    auto node = std::make_shared<OrderController>(); //create the shared pointer of the my node
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();
    rclcpp::shutdown();
    return 0;

}
