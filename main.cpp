// main.cpp
//
// Copyright UTAT 2015
// Author: Winston Liu

#include <opencv2/features2d/features2d.hpp>  
#include <opencv2/nonfree/features2d.hpp>  
#include <opencv2/opencv.hpp>
#include <stdlib.h>
#include <string>
#include <iostream>

#include "surf/dbscan.h"

#define ACTIVE_CHANNEL 1
#define SAVE_INTERMEDIATE_IMAGES

#ifdef SAVE_INTERMEDIATE_IMAGES
#define IMOUT( filename , mat_im ) do { cv::imwrite(filename, mat_im); } while (0)
#else
#define IMOUT( filename, mat_im )
#endif

float DBSCAN_eps = 200;
int DBSCAN_minPts= 2;
// ROI is at most 1/10 of full-sized image
int roiProportionalSize = 10;

cv::Point2i getMean(std::vector<cv::KeyPoint>& subsetKeys)
{
	if (subsetKeys.empty() == true)
		return cv::Point2i(0, 0);
	cv::Point2i meanpt(0, 0);
	int greatest_dist = 0;
	int maxsize = 0;

	// Calculate mean
	for (int i = 0; i < subsetKeys.size(); ++i) 
	{ 
		meanpt.x += subsetKeys[i].pt.x; 
		meanpt.y += subsetKeys[i].pt.y; 

	}
	meanpt.x /= subsetKeys.size();
	meanpt.y /= subsetKeys.size();

	return meanpt;
}

std::vector<cv::KeyPoint> procImWithSurf(cv::Mat raw_input, int SURF_thresh)
{
	cv::Mat outim, channels[3];

	// Convert to HSV
	cv::Mat hsvchannel, hsvim;
    cv::cvtColor(raw_input, hsvim, CV_RGB2HSV); 
    cv::split(hsvim, channels);
    IMOUT("hsvim.jpg", hsvim);
    hsvchannel = channels[ACTIVE_CHANNEL];

	// Denoise
    cv::blur(hsvchannel, hsvchannel, cv::Size(10,10));
    IMOUT("blurred.jpg", hsvchannel);

	// Default is ( ~, 4, 2, true, false)
	// See http://docs.opencv.org/modules/nonfree/doc/feature_detection.html
    cv::Mat mask;
	std::vector<cv::KeyPoint> keypoints;
    cv::SURF mySurf(SURF_thresh, 4, 2, true, true);
    mySurf(hsvchannel, mask, keypoints);
    IMOUT("surf.jpg", hsvchannel);

	// Print keypoints
    cv::drawKeypoints(hsvchannel, keypoints, outim, (0, 255, 0), 4);
    IMOUT("keypoints.jpg", hsvchannel);
	IMOUT("imout.jpg", outim);

    return keypoints;
}

cv::Rect getROISize(cv::Size image_dim, cv::Point2i cluster_mean, int rps)
{
    // rps: ROI size proportional to whole image 
    // Get ROI size as function of image size
    cv::Point2i roi;
    roi.x = image_dim.width / rps;
    roi.y = image_dim.height / rps;

    // Set mean coordinates to the upper left corner
    cluster_mean.x -= roi.x / 2;
    cluster_mean.y -= roi.y / 2;

    // Clamp to image range
    if (cluster_mean.x < 0)
        cluster_mean.x = 0;
    else if (cluster_mean.x + roi.x > image_dim.width) 
        roi.x = image_dim.width - cluster_mean.x;

    if (cluster_mean.y < 0)
        cluster_mean.y = 0;
    else if (cluster_mean.y + roi.y > image_dim.height) 
        roi.y = image_dim.height - cluster_mean.y;

    return cv::Rect(cluster_mean.x, cluster_mean.y, roi.x, roi.y);
}

int main (int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cout << 
			"./image_proc <file> <thresh> (-1 for default)" << std::endl;
		return -1;
	}

    // Get image
	cv::Mat test_im = cv::imread(argv[1], CV_LOAD_IMAGE_COLOR);
    std::vector<cv::KeyPoint> keypoints = procImWithSurf(test_im, atoi(argv[2]));

    // DBSCAN -> returns a tree of vectors
	std::vector<std::vector<cv::KeyPoint> > clusters = DBSCAN_keypoints(&keypoints, DBSCAN_eps, DBSCAN_minPts);
	
	for (int i = 0; i < clusters.size(); ++i)
	{
        // Calculate mean
        cv::Point2i mean = getMean(clusters[i]);

        if (mean.x == 0 && mean.y == 0)
            continue;

        cv::Rect roiBounds = getROISize(test_im.size(), mean, roiProportionalSize);

        // Display ROI
        // Name image
        std::string raw_name(argv[1]); 
        int firstindex = raw_name.find_last_of("/");
        int lastindex = raw_name.find_last_of(".");
        std::string proc_name = raw_name.substr(firstindex + 1, lastindex);

        std::ostringstream oss;
        oss << "ROI_" << proc_name << "_" << i << ".jpg";
        cv::Mat roiOut = test_im(roiBounds).clone();
        IMOUT(oss.str(), roiOut);
	}
}
