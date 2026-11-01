#include "rclcpp/rclcpp.hpp" //importing the ros2 lib if it shows a error open vs from the terminal
#include "std_msgs/msg/bool.hpp" 
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include "deliverbot_msgs/msg/cordinate.hpp"

using Cordinate = deliverbot_msgs::msg::Cordinate;
using Goal =geometry_msgs::msg::PoseStamped;
using Bool = std_msgs::msg::Bool;
using Float64 = std_msgs::msg::Float64MultiArray;

using namespace std::placeholders;
class DoorController : public rclcpp::Node 
{
    public: //puplic
        DoorController() : Node("door_controller") 
        {

            this->position_feedback_sub_ = this->create_subscription<Goal>
            ("current_pose",10,std::bind(&DoorController::feedback_position_callback,this,_1));

            this->cloud_door_command_sub_ = this->create_subscription<Bool>
            ("door_command",10,std::bind(&DoorController::cloud_door_oredr_callback,this,_1));

            this->nav2_done_sub_ = this->create_subscription<Bool>
            ("nav2_done",10,std::bind(&DoorController::nav2_done_callback,this,_1));

            this->door_feedback_pub_ = this->create_publisher<Bool>("door_feedback",10);

            this->door_commander_pub_ = this->create_publisher<Float64>("opendoor",10);
           
            this->glob_nav_done = false;

            this->set_goal(&this->home_,0.0,0.0,0.0);

            this->glob_current_pose_.pose.position.x=0;
            this->glob_current_pose_.pose.position.y=0;
            RCLCPP_INFO(this->get_logger(),"door control Node Is Running");
        }   
    private:
    //define the v of the class and the private methods

    void feedback_position_callback(Goal current_postion)
    {
        this->glob_current_pose_ =current_postion;
        
    }

    void cloud_door_oredr_callback(Bool command)
    {
        if (!command.data)
        {
            Float64 command;
            command.data.push_back(0.0);
            this->door_commander_pub_->publish(command);

            Bool state;
            state.data = false;
            this->door_feedback_pub_->publish(state);
        }
    }

    void nav2_done_callback(Bool nav2_state)
    {
        this->glob_nav_done = nav2_state.data;

        auto x = this->glob_current_pose_.pose.position.x;
        auto y = this->glob_current_pose_.pose.position.y;

        auto distance_to_home = std::sqrt(x*x + y*y);

        if (this->glob_nav_done)
        
        {   RCLCPP_INFO(this->get_logger(), "glob_nav is done");
            this->glob_nav_done = false;
            if (distance_to_home > 0.3)
            {
                Float64 command;
                command.data.push_back(-1.2);
                this->door_commander_pub_->publish(command);

                Bool state;
                state.data = true;
                this->door_feedback_pub_->publish(state);
                RCLCPP_INFO(this->get_logger(), "Arrived at location → opening door");
            }
           else RCLCPP_INFO(this->get_logger(), "Arrived at home → door stays closed");
            
        }
    }

    void set_goal(Cordinate* goal,float x ,float y, float z)
    {
        goal->x = x;
        goal->y = y;
        goal->theta = z;
    }

    

    rclcpp::Subscription<Goal>::SharedPtr position_feedback_sub_;
    rclcpp::Subscription<Bool>::SharedPtr cloud_door_command_sub_;
    rclcpp::Publisher<Float64>::SharedPtr door_commander_pub_;
    rclcpp::Subscription<Bool>::SharedPtr nav2_done_sub_;
    rclcpp::Publisher<Bool>::SharedPtr door_feedback_pub_;

    Cordinate home_;
    bool glob_nav_done;
    Goal glob_current_pose_;
};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);
    auto node = std::make_shared<DoorController>(); //create the shared pointer of the my node
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;

}
