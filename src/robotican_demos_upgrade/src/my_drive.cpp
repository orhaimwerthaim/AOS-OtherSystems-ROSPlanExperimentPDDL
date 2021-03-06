#include <std_srvs/SetBool.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <geometry_msgs/PointStamped.h>
#include <std_srvs/Trigger.h>
#include <tf/transform_listener.h>
#include <actionlib/client/simple_action_client.h>
#include <moveit/planning_scene_interface/planning_scene_interface.h>

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

moveit::planning_interface::PlanningSceneInterface *planning_scene_interface_ptr;
tf::TransformListener *listener_ptr;

move_base_msgs::MoveBaseGoal get_pre_pick_pose();

bool base_cmd(move_base_msgs::MoveBaseGoal goal);

std::string object_name;

MoveBaseClient *moveBaseClient_ptr;

bool moving=false;

double base_distance_from_object=0.55;

move_base_msgs::MoveBaseGoal get_pre_pick_pose(geometry_msgs::Point point) {
    tf::Transform dest_transform;
    move_base_msgs::MoveBaseGoal goal;
    try{

        tf::StampedTransform transform_base;
        try {
            listener_ptr->lookupTransform("map", "base_link", ros::Time(0), transform_base);

        }
        catch (tf::TransformException ex){
            ROS_ERROR("%s",ex.what());
            return goal;
        }
        tf::Vector3 v_obj(point.x,point.y,point.z);

        tf::Vector3 v_base =transform_base.getOrigin();

        tf::Vector3 v=v_obj-v_base;
        double yaw=atan2(v.y(),v.x());
        double away=base_distance_from_object/sqrt(v.x()*v.x()+v.y()*v.y());
        tf::Vector3 dest=v_base+v*(1-away);

        dest_transform.setOrigin( dest );
        dest.setZ(0);
        tf::Quaternion q;
        q.setRPY(0.0, 0, yaw);
        dest_transform.setRotation(q);

        //std::cout<< map_object_pose.pose.position<<std::endl;

    }

    catch (tf::TransformException ex){
        ROS_ERROR("%s",ex.what());
        return goal;
    }

    goal.target_pose.header.frame_id = "map";
    goal.target_pose.header.stamp = ros::Time::now();
    goal.target_pose.pose.position.x=dest_transform.getOrigin().x();
    goal.target_pose.pose.position.y=dest_transform.getOrigin().y();
    std::cout<< goal.target_pose.pose.position.x<<std::endl;
    goal.target_pose.pose.position.z=0;
    goal.target_pose.pose.orientation.x=dest_transform.getRotation().x();
    goal.target_pose.pose.orientation.y=dest_transform.getRotation().y();
    goal.target_pose.pose.orientation.z=dest_transform.getRotation().z();
    goal.target_pose.pose.orientation.w=dest_transform.getRotation().w();
    return goal;
}

bool drive_go_cb(std_srvs::Trigger::Request  &req,
                 std_srvs::Trigger::Response &res)
{
    std_srvs::SetBool srv;

    std::vector<std::string> ids;
    ids.push_back(object_name);
    std::map<std::string, moveit_msgs::CollisionObject> poses=planning_scene_interface_ptr->getObjects(ids);
    std::map<std::string, moveit_msgs::CollisionObject>::iterator it;

    it = poses.find(object_name);

    if (it != poses.end()) {
        moveit_msgs::CollisionObject obj=it->second;

        if (!moving) moving=true;

        move_base_msgs::MoveBaseGoal goal=get_pre_pick_pose(obj.primitive_poses[0].position);

        ROS_INFO_STREAM(obj.primitive_poses[0].position);
        //ROS_INFO("%s\n", obj.primitive_poses[0].position);

        srv.request.data=false;

        if (base_cmd(goal)) {
            ROS_INFO("Reached position");
            res.message="Reached pre-picking position";
            res.success=true;
        }
        else{
            ROS_INFO("move_base failed");
            res.message="move_base failed";
            res.success=false;
        }
        moving=false;
        srv.request.data=true;
        return true;
    }
    else {
        ROS_ERROR("No object found");
        res.message="No object found";
        res.success=false;
        srv.request.data=true;
        return true;
    }

}

bool base_cmd(move_base_msgs::MoveBaseGoal goal) {

    ROS_INFO("[%s]: Sending goal", ros::this_node::getName().c_str());

    //ros::NodeHandle n;
    //ros::Publisher _chatter_pub = n.advertise<std_msgs::String>("/robot_state",10);
    //ros::Duration(1).sleep();

    //std_msgs::String msg;
    //msg.data = "go";
    //_chatter_pub.publish(msg);

    moveBaseClient_ptr->sendGoal(goal);
    moveBaseClient_ptr->waitForResult();

    if(moveBaseClient_ptr->getState() == actionlib::SimpleClientGoalState::SUCCEEDED) {
        return true;
    }
    else {
        ROS_ERROR("[%s]: Navigation failed ", ros::this_node::getName().c_str());
        return false;
    }
}

int main(int argc, char **argv) {

    ros::init(argc, argv, "my_drive2object_node");
    ros::NodeHandle n;
    ros::NodeHandle pn("~");


    pn.param<double>("base_distance_from_object", base_distance_from_object, 0.55);
    pn.param<std::string>("object_name", object_name, "can");

    MoveBaseClient moveBaseClient("move_base", true);
    //wait for the action server to come up
    while(!moveBaseClient.waitForServer(ros::Duration(5.0))){
        ROS_INFO("Waiting for the move_base action server to come up");
    }

    moveBaseClient_ptr=&moveBaseClient;

    ros::ServiceServer service = n.advertiseService("my_drive2object_go", drive_go_cb);

    tf::TransformListener listener;
    listener_ptr=&listener;

    moveit::planning_interface::PlanningSceneInterface planning_scene_interface;
    planning_scene_interface_ptr=&planning_scene_interface;

    ros::ServiceClient uc_client = n.serviceClient<std_srvs::SetBool>("update_collision_objects");
    ROS_INFO("Waiting for update_collision service...");
    uc_client.waitForExistence();
    std_srvs::SetBool srv;
    srv.request.data=true;
    uc_client.call(srv);

    ros::spin();

    return 0;
}
