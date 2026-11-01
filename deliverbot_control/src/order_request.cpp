#include "rclcpp/rclcpp.hpp" //importing the ros2 lib if it shows a error open vs from the terminal
#include "rclcpp_action/rclcpp_action.hpp"
#include "deliverbot_msgs/action/order_request.hpp"
#include "deliverbot_msgs/msg/order_req.hpp"

using OrderReq = deliverbot_msgs::msg::OrderReq;
using OrderRequestAction = deliverbot_msgs::action::OrderRequest;
using OrderControllerGoalHandler = rclcpp_action::ClientGoalHandle<OrderRequestAction>;

using namespace std::placeholders;
class OrderRequest : public rclcpp::Node //create a class and inharet from the rclcpp
{
    public: //puplic
        OrderRequest() : Node("order_request") 
        {

            this->cloude_orders_ = this->create_subscription<OrderReq>
            ("order_req",10,std::bind(&OrderRequest::cloude_requests_callback, this,_1));

            this->order_action_clint_ = rclcpp_action::create_client<OrderRequestAction>(this,"order_request");

            
            RCLCPP_INFO(this->get_logger(),"order request Node Is Running");
        }   
    private:
    //define the v of the class and the private methods

    void cloude_requests_callback(OrderReq order)
    {
        this->order_action_clint_->wait_for_action_server();

        auto goal = OrderRequestAction::Goal();
        goal.start = order.start;
        goal.end = order.end;
        RCLCPP_INFO(this->get_logger(),"receiving goals start:%s , end:%s",
        goal.start.c_str(),goal.end.c_str());
        auto options = rclcpp_action::Client<OrderRequestAction>::SendGoalOptions();

        options.result_callback = std::bind(&OrderRequest::result_callback,this,_1);
        options.goal_response_callback = std::bind(&OrderRequest::goal_responce_callback,this,_1);
        options.feedback_callback = std::bind(&OrderRequest::feedback_goal_callback,this,_1,_2);

        RCLCPP_INFO(this->get_logger(),"sending the goal...") ;
        this->order_action_clint_->async_send_goal(goal,options);

    }

    void result_callback(const OrderControllerGoalHandler::WrappedResult &result)
    {
        auto status = result.code;
        if(status == rclcpp_action::ResultCode::SUCCEEDED){
            
            RCLCPP_INFO(this->get_logger(),"Succeeded");
        }
        else if (status == rclcpp_action::ResultCode::ABORTED)
        {
            RCLCPP_ERROR(this->get_logger(),"Aborted");
        }
        else if (status == rclcpp_action::ResultCode::CANCELED)
        {
            RCLCPP_ERROR(this->get_logger(),"Canceld");
        }
        
        auto result_name = result.result->msg ;
        RCLCPP_INFO(this->get_logger(),"Result: %s",result_name.c_str()) ;
    }

    void feedback_goal_callback(const OrderControllerGoalHandler::SharedPtr &goal_handle ,
            const std::shared_ptr<const OrderRequestAction::Feedback> feedback)
    {
        (void)goal_handle;
        std::string pose = feedback->position;
        RCLCPP_INFO(this->get_logger(),"Feedback: %s",pose.c_str());
    }

    void goal_responce_callback(const OrderControllerGoalHandler::SharedPtr &goal_handel)
    {
        if (! goal_handel)
        {
            RCLCPP_INFO(this->get_logger(),"Goal has been rejected");
        }
        else
        {
            this->goal_handler = goal_handel;
            RCLCPP_INFO(this->get_logger(),"Goal has been acepted");
        }
    }
    OrderControllerGoalHandler::SharedPtr goal_handler;
    rclcpp::Subscription<OrderReq>::SharedPtr cloude_orders_;
    rclcpp_action::Client<OrderRequestAction>::SharedPtr order_action_clint_;
};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);
    auto node = std::make_shared<OrderRequest>(); //create the shared pointer of the my node
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;

}
