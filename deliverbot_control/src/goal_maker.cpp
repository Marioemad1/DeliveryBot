#include "rclcpp/rclcpp.hpp"
#include "deliverbot_msgs/msg/cordinate.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "tf2/LinearMath/Quaternion.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

using Cordinate = deliverbot_msgs::msg::Cordinate;
using Goal = geometry_msgs::msg::PoseStamped;

using namespace std::placeholders ;

class GoalMaker : public rclcpp::Node //create a class and inharet from the rclcpp
{
    public: //puplic
        GoalMaker() : Node("goal_maker") 
        {
            this->goal_sender_ = this->create_publisher<Goal>("send_goal",10);

            this->goal_recive_ = this->create_subscription<Cordinate>("create_goal",10,
            std::bind(&GoalMaker::recive_goal_callback,this,_1));

            Goal init;
            this->wanted_goal_ = init;

            RCLCPP_INFO(this->get_logger(),"goal Maker is running");
        }   
    private:
    //define the v of the class and the private methods

    void recive_goal_callback(const Cordinate::SharedPtr goal)
    {
        tf2::Quaternion q;
        q.setRPY(0.0,0.0,goal->theta);


        this->wanted_goal_.header.frame_id = "map";
        this->wanted_goal_.header.stamp = this->get_clock()->now();
        this->wanted_goal_.pose.position.x = goal->x;
        this->wanted_goal_.pose.position.y = goal->y;
        this->wanted_goal_.pose.orientation = tf2::toMsg(q);

        this->goal_sender_->publish(wanted_goal_);
        RCLCPP_INFO(this->get_logger(),"Sending x:%f y:%f z:%f to nav2",goal->x,goal->y,goal->theta);
    }

    Goal wanted_goal_;

    rclcpp::Publisher<Goal>::SharedPtr goal_sender_;
    rclcpp::Subscription<Cordinate>::SharedPtr goal_recive_;
};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);
    auto node = std::make_shared<GoalMaker>(); //create the shared pointer of the my node
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;

}
