
#include <terrain_slam/clouds.h>

#include <opencv2/opencv.hpp>

#include <pcl/common/transforms.h>
#include <pcl/sample_consensus/sac_model_line.h>
#include <pcl/sample_consensus/ransac.h>

void terrain_slam::CloudPatch::add(const CloudPatch& other_patch) {
  Eigen::Matrix4d tf_other = other_patch.transform();
  Eigen::Matrix4d tf = transform().inverse()*tf_other;
  Cloud transformed;
  pcl::transformPointCloud(*other_patch.cloud, transformed, tf.cast<float>());
  add(transformed);
}

void terrain_slam::CloudPatch::add(const Cloud& other_cloud) {
  *cloud += other_cloud;
}

void terrain_slam::CloudPatch::add(const Point& other_point) {
  cloud->push_back(other_point);
}

void terrain_slam::CloudPatch::add(const Eigen::Vector3d& other_point) {
  Point p(other_point(0), other_point(1), other_point(2));
  cloud->push_back(p);
}

void terrain_slam::CloudPatch::add(const Eigen::Vector4d& other_point) {
  Point p(other_point(0), other_point(1), other_point(2));
  cloud->push_back(p);
}

Eigen::Vector3d terrain_slam::CloudPatch::getCentroid() const {
  Eigen::Vector3d pe;
  for (size_t i = 0; i < cloud->size(); i++) {
    Point pt = cloud->points[i];
    pe += Eigen::Vector3d(pt.x, pt.y, pt.z);
  }
  return pe/cloud->size();
}

std::vector<Eigen::Vector4d>
terrain_slam::CloudPatch::kNN2d(const Eigen::Vector4d& p, int n) const {
  std::vector<Eigen::Vector4d> points;
  std::vector<int> point_idx(n);
  std::vector<float> point_sq_distance(n);
  pcl::PointXY sp;
  sp.x = p(0);
  sp.y = p(1);
  kdtree2d.nearestKSearch(sp, n, point_idx, point_sq_distance);
  for (size_t i = 0; i < point_idx.size(); i++) {
    Point pt = cloud->points[point_idx[i]];
    points.push_back(Eigen::Vector4d(pt.x, pt.y, pt.z, 1.0));
  }
  return points;
}

std::vector<Eigen::Vector4d>
terrain_slam::CloudPatch::kNN(const Eigen::Vector4d& p, int n) const {
  std::vector<Eigen::Vector4d> points;
  std::vector<int> point_idx(n);
  std::vector<float> point_sq_distance(n);
  Point sp(p(0), p(1), p(2));
  kdtree.nearestKSearch(sp, n, point_idx, point_sq_distance);
  for (size_t i = 0; i < point_idx.size(); i++) {
    Point pt = cloud->points[point_idx[i]];
    points.push_back(Eigen::Vector4d(pt.x, pt.y, pt.z, 1.0));
  }
  return points;
}

void terrain_slam::CloudPatch::fitLine(double &stddev) const {
  // Create a shared line model pointer directly
  pcl::SampleConsensusModelLine<pcl::PointXYZ>::Ptr model(new pcl::SampleConsensusModelLine<pcl::PointXYZ>(cloud));

  // Create the RANSAC object
  pcl::RandomSampleConsensus<pcl::PointXYZ> sac(model, 1.5);

  // Algorithm tests
  bool result = sac.computeModel();

  std::vector<int> sample;
  sac.getModel(sample);

  std::vector<int> inliers;
  sac.getInliers(inliers);

  Eigen::VectorXf coeff;
  sac.getModelCoefficients(coeff);

  Eigen::VectorXf coeff_refined;
  model->optimizeModelCoefficients(inliers, coeff, coeff_refined);

  std::vector<double> sq_dist;
  model->getDistancesToModel(coeff_refined, sq_dist);

  double mean;
  std::vector<float> sq_dist_f(sq_dist.begin(), sq_dist.end());
  pcl::getMeanStd(sq_dist_f, mean, stddev);




  // cv::namedWindow("test", 0);
  // cv::Mat img = cv::Mat::zeros(1000, 1000, CV_8UC3); // -10 to 10 m in y, 2 to 18m in z
  // for (size_t i = 0; i < cloud->points.size(); i++) {
  //   double y = cloud->points[i].y;
  //   double z = cloud->points[i].z;
  //   int r = ((z - 2.0) / 16.0)*1000;
  //   if (r < 0) r = 0;
  //   int c = (500 + y/8.0*500);
  //   img.at<cv::Vec3b>(r, c) = cv::Vec3b(0, 0, 255);
  // }
  // double t = (8.0-coeff_refined[1])/coeff_refined[4];
  // cv::Point2d p1(8.0, coeff_refined[2]+coeff_refined[5]*t);
  // t = (-8.0-coeff_refined[1])/coeff_refined[4];
  // cv::Point2d p2(-8.0, coeff_refined[2]+coeff_refined[5]*t);


  // cv::Point pt1, pt2;
  // pt1.x = (500 + p1.x/8.0*500);
  // pt1.y = ((p1.y - 2.0) / 16.0)*1000;
  // pt2.x = (500 + p2.x/8.0*500);
  // pt2.y = ((p2.y - 2.0) / 16.0)*1000;
  // cv::line(img, pt1, pt2, cv::Scalar(0, 255, 0));
  // cv::imshow("test", img);
  // cv::waitKey(1);


  // // Create the filtering object
  // pcl::ExtractIndices<pcl::PointXYZ> extract;
  // // Extract the inliers
  // extract.setInputCloud (cloud);
  // extract.setIndices (inliers);
  // extract.setNegative (false);
  // extract.filter (*inliers_cloud);
}

void terrain_slam::CloudPatch::getMeanStd(double &mean, double &stddev) const {
  std::vector<float> z_values2;
  for(size_t i=0; i<cloud->points.size(); i++) {
    z_values2.push_back(cloud->points[i].z);
  }
  pcl::getMeanStd(z_values2, mean, stddev);
  std::vector<float> z_values;
  for(size_t i=0; i<z_values2.size(); i++) {
    if ( (z_values2[i] - mean) < 3*stddev)
      z_values.push_back(z_values2[i]);
  }
  pcl::getMeanStd(z_values, mean, stddev);
  // printf("z mean: %03.3f, stddev: %03.3f\n", mean, stddev);
}