#include "rclcpp/rclcpp.hpp" 
#include <mosquitto.h>
#include <string>
#include "deliverbot_msgs/msg/cordinate.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "std_msgs/msg/bool.hpp"
#include <nlohmann/json.hpp>
#include "deliverbot_msgs/msg/order_req.hpp"

using Json = nlohmann::json;

using OrderReq = deliverbot_msgs::msg::OrderReq;
using Bool = std_msgs::msg::Bool;
using Cordinate = deliverbot_msgs::msg::Cordinate;
using Goal =geometry_msgs::msg::PoseStamped;

using namespace std::placeholders;

class THingsBoardCloudNode : public rclcpp::Node 
{
    public: //puplic
        THingsBoardCloudNode() : Node("things_board_node") 
        {
            mosquitto_lib_init();
            mosq_ = mosquitto_new("deliveryrobot",1,this);
            mosquitto_username_pw_set(mosq_,"dDnULdjZtBuAqTe15G5t",NULL);  
            
            mosquitto_message_callback_set(mosq_,&THingsBoardCloudNode::cloud_callback);
            mosquitto_connect(mosq_, "mqtt.thingsboard.cloud", 1883, 60);
            mosquitto_subscribe(mosq_, NULL, "v1/devices/me/attributes", 1);
            mosquitto_subscribe(mosq_, NULL, "v1/devices/me/rpc/request/+", 1);
        
            mosquitto_loop_start(mosq_);

            
            

            this->last_telemetry_time_ = this->now();
            this->door_commander_ = this->create_publisher<Bool>("door_command",10);

            this->door_feedback_ = this->create_subscription<Bool>("door_feedback",10,
            std::bind(&THingsBoardCloudNode::door_feedback,this,_1));

            this->order_publisher_ = this->create_publisher<OrderReq>("order_req",10);

            this->feedback_ = this->create_subscription<Goal>("current_pose",10,
            std::bind(&THingsBoardCloudNode::feedback_callback,this,_1));

            RCLCPP_INFO(this->get_logger(),"Things Board Node Is Running");

        }

        ~THingsBoardCloudNode()
        {
            mosquitto_loop_stop(mosq_, true);
            mosquitto_disconnect(mosq_);
            mosquitto_destroy(mosq_);
            mosquitto_lib_cleanup();
        }
    private:
    //define the v of the class and the private methods
    struct mosquitto *mosq_;
    
    

    static void cloud_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
    {
        (void)mosq;

        auto *self = static_cast<THingsBoardCloudNode *>(obj);
        // std::string data(static_cast<char *>(msg->payload), msg->payloadlen);
        // RCLCPP_INFO(self->get_logger(), "Cloud says: %s", data.c_str());

        std::string topic(msg->topic);
        std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);

        RCLCPP_INFO(self->get_logger(),"The Topic: %s",topic.c_str());

        try
        {
            Json data =Json::parse(payload);

            if (topic.find("v1/devices/me/rpc/request/") != std::string::npos)
            {
                std::string method = data.value("method","");
                auto params = data["params"];
                std::string request_id = topic.substr(topic.find_last_of('/') + 1);
                
                if (method == "setDoor")
                {
                    Bool open;
                    open.data = false;
                    if (params.is_boolean()) open.data = params.get<bool>();
                    self->door_commander_->publish(open);
                    RCLCPP_INFO(self->get_logger(),"the door oredr is:%d",(int)open.data);
                    std::string response = open.data ? "true" : "false";
                    std::string response_topic = "v1/devices/me/rpc/response/" + request_id;

                    std::lock_guard<std::mutex> lock(self->mqtt_mutex_);
                    int rc = mosquitto_publish(mosq, NULL, response_topic.c_str(),
                                  response.length(), response.c_str(), 1, false);
                    if (rc != MOSQ_ERR_SUCCESS)
                    {
                        RCLCPP_WARN(self->get_logger(),"telementry publish faild: %s",mosquitto_strerror(rc));
                    }
                    RCLCPP_INFO(self->get_logger(),"the door oredr is:%s",response.c_str());

                }
                
            }
            else if (topic.find("v1/devices/me/attributes") != std::string::npos)
            {
  
                if (data.contains("pickup") && data.contains("placeoff"))
                {
                    OrderReq order;
                    order.start = data["pickup"];
                    order.end = data["placeoff"];
                    RCLCPP_INFO(self->get_logger(),"the goals is start:%s , end:%s",
                    order.start.c_str(),order.end.c_str());
                    self->order_publisher_->publish(order);
                }
                
                
            }
            
        }
        catch(const std::exception& e)
        {
            RCLCPP_ERROR(self->get_logger(), "JSON Error: %s", e.what());
        }
        
        
    }

    void feedback_callback( Goal::SharedPtr current_pose)
    {
        auto now = this->now();

        if ((now - this->last_telemetry_time_).seconds() < 0.25)
        {
            return;
        }

        last_telemetry_time_ = now;
        float x = current_pose->pose.position.x;
        float y = current_pose->pose.position.y;

        float thingx = (15.154 - x) / 30.308; //to convert to a number from 0.5 to 1 
        float thingy = (12.157 - y) / 24.314; //to convert to a number from 0.5 to 1 

        //don't forget to configure things board on the same scale 

        std::string msg = "{\"xPos\":" + std::to_string(thingy) + ",\"yPos\":"+ std::to_string(thingx) + "}";

        std::lock_guard<std::mutex> lock(mqtt_mutex_);
        int rc = mosquitto_publish(mosq_, NULL, "v1/devices/me/telemetry",
                  msg.length(), msg.c_str(), 1, false);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            RCLCPP_WARN(this->get_logger(),"telementry publish faild: %s",mosquitto_strerror(rc));
        }
        
    }

    void door_feedback (Bool::SharedPtr door_state)
    {
        (void) door_state;
        return;

    }

    rclcpp::Subscription<Goal>::SharedPtr feedback_;
    rclcpp::Publisher<Bool>::SharedPtr door_commander_;
    rclcpp::Subscription<Bool>::SharedPtr door_feedback_;
    rclcpp::Publisher<OrderReq>::SharedPtr order_publisher_;

    std::mutex mqtt_mutex_;

    rclcpp::Time last_telemetry_time_;


};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);
    auto node = std::make_shared<THingsBoardCloudNode>(); //create the shared pointer of the my node
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;

}
