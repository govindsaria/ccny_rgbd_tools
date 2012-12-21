#ifndef CCNY_RGBD_MONO_VO
#define CCNY_RGBD_MONO_VO

#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/image_encodings.h>
#include <std_msgs/Time.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TwistStamped.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <pcl/point_types.h>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/registration/icp.h>
#include <pcl/registration/correspondence_estimation.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl_ros/point_cloud.h>
#include <image_transport/image_transport.h>
#include <image_transport/subscriber_filter.h>
#include <image_geometry/pinhole_camera_model.h>
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <cv_bridge/cv_bridge.h>

#include "ccny_rgbd/types.h"
#include "ccny_rgbd/rgbd_util.h"
#include "ccny_rgbd/structures/rgbd_keyframe.h"

namespace ccny_rgbd
{

using namespace message_filters::sync_policies;

class MonocularVisualOdometry
{
  typedef nav_msgs::Odometry OdomMsg;

  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    MonocularVisualOdometry(ros::NodeHandle nh, ros::NodeHandle nh_private);
    virtual ~MonocularVisualOdometry();
    /**
     * @brief Fitness function for RANSAC used to find the initial camera pose
     *
     * @param M the intrinsic 3x3 camera matrix
     * @param E the extrinsic 3x4 camera matrix (composed by rotation and translation parameters)
     * @param distance_threshold Fitness criterion for determining a good fit if distance (in pixels) between a nearest-neighbors pair is less than or equal to this threshold
     * @param min_inliers The minimum number of inliers as a criterion for determining a good fit of the hypothesis
     * @param sample_3D_points Random sample of 6 3D points from the sparse cloud  mode
     * @param feature_2D_points Detected 2D features in input image
     * @param inliers_3D_points Resulting inliers from the 3D point cloud model (pointer as member variable $model_ptr_$)
     * @param inliers_2D_points Resulting inliers from the set of 2D point features
     *
     * @return True if the fitness falls under certain threshold criteria of number of inliers
     */
    bool fitness(const cv::Mat M, const cv::Mat E, const int distance_threshold, const int min_inliers, const std::vector<cv::Point3d> &sample_3D_points, const std::vector<cv::Point2d> & feature_2D_points, std::vector<cv::Point3d> &inliers_3D_points, std::vector<cv::Point2d> & inliers_2D_points);

   // estimate the first camera pose
   void estimateFirstPose(
     const Matrix3f& intrinsic_matrix,
     Matrix3f& rmat, // Output 3x3 rotation matrix
     Vector3f& tvec, // Output 3x1 translation vector
     const PointCloudT::Ptr& cloud,
     int min_inliers, 
     int max_iterations,
     int distance_threshold);

   /**
    * @brief Fitness function for RANSAC used to find the initial camera pose
    *
    * @param model_3D the 3D point cloud model as a vector
    * @param features_2D the 2D keypoints (features) on the current frame
    * @param E the 3x4 extrinsic matrix
    * @param corr_3D_points the vector of 3D points corresponding to the 2D points found
    * @param corr_2D_points the vector of 2D points correspondances to the 2D keypoints (features detetected on frame)
    *
    * @return whether true/false correspondences were found (NOT: The normalized accumulated distances (error) of the correspondences found)
    */
   bool getCorrespondences(
    const std::vector<cv::Point3d> &model_3D, 
    const std::vector<cv::Point2d> &features_2D, 
    const cv::Mat &E, std::vector<cv::Point3d> &corr_3D_points, 
    std::vector<cv::Point2d> &corr_2D_points,
    bool last_iteration);

   void estimateMotion(
                       Matrix3f& rmat, // Input/Output 3x3 rotation matrix
                       Vector3f& tvec, // Input/Output 3x1 translation vector
                       const PointCloudT::Ptr& model,
                       int max_PnP_iterations = 10);

  private:

    // **** ROS-related

    ros::NodeHandle nh_;
    ros::NodeHandle nh_private_;
    tf::TransformListener tf_listener_;
    tf::TransformBroadcaster tf_broadcaster_;
    ros::Publisher odom_publisher_;

//    boost::shared_ptr<image_transport::ImageTransport> rgb_it_;
    boost::shared_ptr<SynchronizerMonoVO> sync_;
    image_geometry::PinholeCameraModel cam_model_;
    Matrix3f intrinsic_matrix_;

    ImageSubFilter      sub_rgb_;
    CameraInfoSubFilter sub_info_;

    // **** parameters 

    std::string pcd_filename_;
    std::string fixed_frame_; 
    std::string base_frame_;

    std::string detector_type_;
    std::string descriptor_type_;
    double max_descriptor_space_distance_;

    // **** variables
    boost::mutex::scoped_lock mutex_lock_; ///< Thread lock on subscribed input images
    bool initialized_;
    bool is_first_time_projecting_; ///< To indicate the first instance when the complete cloud model gets projected to the camera
    bool assume_initial_position_; ///< To indicate whether the assumption of known initial position of the camera pose is applied
    bool visualize_correspondences_; ///< To indicate whether correspondeces (matched points) will be vizualized in the frame
    int  frame_count_;
    ros::Time init_time_;

    // PnP parameters
    int number_of_iterations_;
    double reprojection_error_;
    int min_inliers_count_;

    tf::Transform b2c_;
    tf::Transform f2b_;

    cv::Mat E_; ///< the camera's extrinsic matrix
    Matrix3f rmat_; // 3x3 rotation matrix
    Vector3f tvec_; // 3x1 translation vector

//    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_;
    PointCloudT::Ptr model_ptr_;
    ros::Publisher pub_cloud_est_; ///< Publisher for the point cloud model as estimated
    ros::Publisher pub_model_; ///< Publisher for the point cloud model (sparse map)

    image_transport::Publisher virtual_img_pub_;

    bool publish_cloud_model_; ///< to indicate whether the model pointcloud will be published
    bool publish_virtual_img_; ///< to indicate whether the virtual image will be published

    // Topic names:
    std::string topic_cam_info_;
    std::string topic_image_;
    std::string topic_virtual_image_;

    std::string path_to_keyframes_;
    int initial_keyframe_number_;


    // **** private functions
    void testEstimationFromKeyFrames(std::string keyframe_path, int keyframe_number);
    void testEstimationFromVirtualKeyFrames(std::string keyframe_path, int keyframe_number);
    void getVirtualImageFromKeyframe(const PointCloudT& cloud, const Matrix3f& intrinsic, const tf::Transform& extrinsic_tf, cv::Mat& rgb_img, cv::Mat& depth_img);

    std::string formKeyframeName(int keyframe_number, int num_of_chars);
    void generateKeyframePaths(const std::string& keyframe_path, int keyframe_number, std::string& current_keyframe_path, std::string& next_keyframe_path);

    void imageCallback(const ImageMsg::ConstPtr& rgb_msg, const sensor_msgs::CameraInfoConstPtr& info_msg);
    void initParams();
    void publishTransform(const tf::Transform &source2target_transform, const std::string& source_frame_id, const std::string& target_frame_id);
    void publishTransformF2B(const std_msgs::Header& header);

    bool getBaseToCameraTf(const std_msgs::Header& header);
    void setFeatureDetector();
    bool readPointCloudFromPCDFile(); ///< Returns true if PCD file was read successfully.
    
    void testGetMatches();
    bool getMatches(
      cv::flann::Index& kd_tree,
      const cv::Mat& query_points,          // 2d visible query points
      std::vector<int>& match_indices,
      std::vector<float>& match_distances,
      bool prune_repeated_matches = true); ///< return true if matches exist

    void project3DTo2D(const std::vector<cv::Point3d> &input_3D_points,
					             const cv::Mat &extrinsic, 
					             const cv::Mat &intrinsic,
					             std::vector<cv::Point2d> &vector_2D_points);

    void getVisible3DPoints(const std::vector<cv::Point3d> &input_3D_points,
					             const cv::Mat &extrinsic,
					             const cv::Mat &intrinsic,
					             std::vector<cv::Point3d> &visible_3D_points,
					             std::vector<cv::Point2d> &visible_2D_points);
};

} //namespace ccny_rgbd

#endif // CCNY_RGBD_MONO_VO
