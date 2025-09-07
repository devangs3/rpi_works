#include <iostream>
#include <memory>
#include <vector>
#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/stream.h>
#include <libcamera/request.h>
#include <libcamera/framebuffer.h>
#include <opencv2/opencv.hpp>
#include <sys/mman.h>
#include <unistd.h>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace libcamera;

std::mutex mtx;
std::condition_variable cond_var;
bool request_completed = false;

static void requestComplete(Request *request) {
    std::lock_guard<std::mutex> lock(mtx);
    request_completed = true;
    cond_var.notify_one();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rgb|yuv|max_res>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode != "rgb" && mode != "yuv" && mode != "max_res") {
        std::cerr << "Invalid mode. Use 'rgb', 'yuv', or 'max_res'." << std::endl;
        return 1;
    }

    auto cm = std::make_unique<CameraManager>();
    cm->start();

    if (cm->cameras().empty()) {
        std::cerr << "No cameras found." << std::endl;
        return 1;
    }

    std::shared_ptr<Camera> camera = cm->cameras()[0];
    if (camera->acquire()) {
        std::cerr << "Failed to acquire camera." << std::endl;
        return 1;
    }

    std::unique_ptr<CameraConfiguration> config = camera->generateConfiguration({StreamRole::Viewfinder});
    StreamConfiguration &streamConfig = config->at(0);
    
    // Choose the pixel format based on user input
    if (mode == "rgb") {
        // v4l2-ctl shows 'BGR3', which maps to libcamera's formats::BGR888
        streamConfig.pixelFormat = formats::BGR888;
    } else if (mode == "yuv") {
        // v4l2-ctl shows 'YUYV', which maps to libcamera's formats::YUYV
        streamConfig.pixelFormat = formats::YUYV;
    }

    // Set resolution based on mode
    if (mode == "max_res") {
        // The "stepwise" resolution indicates a wide range is possible.
        // Request a very high value and let libcamera negotiate it down.
        streamConfig.size.width = 16376; 
        streamConfig.size.height = 16376;

        // BGR888 is a good default for color processing in OpenCV.
        streamConfig.pixelFormat = formats::BGR888;

        std::cout << "Attempting to use maximum available resolution for " << streamConfig.pixelFormat.toString() << std::endl;
    } else {
        // Use a fixed resolution for testing
        streamConfig.size.width = 640; 
        streamConfig.size.height = 480;
    }

    // After modifying the config, you must validate it to let libcamera find
    // the nearest supported settings.
    config->validate();

    std::cout << "Configured pixel format: " << streamConfig.pixelFormat.toString() << std::endl;
    std::cout << "Negotiated resolution: " << streamConfig.size.width << "x" << streamConfig.size.height << std::endl;

    if (camera->configure(config.get())) {
        std::cerr << "Failed to configure camera" << std::endl;
        return 1;
    }

    FrameBufferAllocator allocator(camera);
    if (allocator.allocate(streamConfig.stream()) < 0) {
        std::cerr << "Failed to allocate buffers" << std::endl;
        return 1;
    }

    Stream *stream = streamConfig.stream();
    std::vector<std::unique_ptr<Request>> requests;
    for (const auto &buffer : allocator.buffers(stream)) {
        requests.push_back(camera->createRequest());
        if (!requests.back()) {
            std::cerr << "Failed to create request." << std::endl;
            return 1;
        }
        if (requests.back()->addBuffer(stream, buffer.get()) < 0) {
            std::cerr << "Failed to add buffer to request." << std::endl;
            return 1;
        }
    }

    camera->requestCompleted.connect(requestComplete);
    camera->start();

    // The main capture loop
    for (auto const &request : requests) {
        if (camera->queueRequest(request.get()) < 0) {
            std::cerr << "Failed to queue request." << std::endl;
            return 1;
        }
        {
            std::unique_lock<std::mutex> lock(mtx);
            cond_var.wait(lock, [] { return request_completed; });
            request_completed = false;
        }

        // Processing the captured frame
        const FrameBuffer *buffer = request->buffers().at(stream);
        const FrameBuffer::Plane &plane = buffer->planes()[0];
        
        // Map the buffer to user space
        void *data = mmap(NULL, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        if (data == MAP_FAILED) {
            std::cerr << "Failed to mmap" << std::endl;
            continue;
        }

        // Create OpenCV Mat
        cv::Mat image;
        if (streamConfig.pixelFormat == formats::BGR888) {
            image = cv::Mat(streamConfig.size.height, streamConfig.size.width, CV_8UC3, data);
        } else if (streamConfig.pixelFormat == formats::YUYV) {
            cv::Mat yuyv_image(streamConfig.size.height, streamConfig.size.width, CV_8UC2, data);
            cv::cvtColor(yuyv_image, image, cv::COLOR_YUV2BGR_YUYV);
        } else {
            std::cerr << "Unsupported format for OpenCV processing." << std::endl;
            munmap(data, plane.length);
            continue;
        }

        // Display the image
        if (!image.empty()) {
            // cv::imshow("Captured Image", image);
            // cv::waitKey(0);
            cv::imwrite("test.jpg",image);
            std::cout << "Saved image..." <<std::endl;
        }

        munmap(data, plane.length);
    }
    
    // Cleanup
    camera->stop();
    camera->release();
    cm->stop();

    return 0;
}