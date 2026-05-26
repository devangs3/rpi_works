#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include <opencv2/opencv.hpp>

// SOLID single responsibility + RAII 
class FrameBuffer{
private:
	std::mutex mtx_;
	cv::Mat frame_;
	bool has_new_{false};

public:
	void update(const cv::Mat& new_frame){
		std::lock_guard<std::mutex> lock(mtx_);
		// Clone without deepcopy for data integrity 
		frame_ = new_frame.clone();
		has_new_=true;
	}
	
	bool get(cv::Mat& output){
		std::lock_guard<std::mutex> lock(mtx_);
		if(!has_new_ || frame_.empty()) return false;
		output =frame_; // shallow copy 
		has_new_=false;
		return true;
	}

};

// SOLID interface segregation and dependency inversion 
class ICameraStream{
public:
	virtual ~ICameraStream() = default;
	virtual void start() =0;
	virtual void stop() =0;
};

// RAII for capture 
class JetsonCameraStream : public ICameraStream{
public:
	explicit JetsonCameraStream(int sensor_id, FrameBuffer& buffer): 
		sensor_id_(sensor_id), buffer_(buffer), running_(false) {}

	~JetsonCameraStream() override{
		stop();
	}

	// Strict RAII ownership using delete copy of object 
	JetsonCameraStream(const JetsonCameraStream&)=delete;
	JetsonCameraStream& operator =(const JetsonCameraStream&)=delete;

	void start() override{
		if (running_) return;
		running_ = true;
		worker_thread_ = std::thread(&JetsonCameraStream::capture_loop, this);
	}	

	void stop() override{
		if(!running_) return;
		running_ = false;
		if(worker_thread_.joinable()){
			worker_thread_.join();
		}
	}
private:
	void capture_loop(){
		  std::string pipeline = 
            "nvarguscamerasrc sensor-id=" + std::to_string(sensor_id_) + " ! "
            "video/x-raw(memory:NVMM), width=1280, height=720, format=NV12, framerate=60/1 ! "
            "nvvidconv ! video/x-raw, format=BGRx ! "
            "videoconvert ! video/x-raw, format=BGR ! appsink drop=true max-buffers=1 sync=false";

          cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
          if(!cap.isOpened()){
          	std::cerr<<"[Error] Failed to open camera channel: " << sensor_id_ << std::endl;
          	return;
          }
          
          cv::Mat frame;			
			while (running_) {
			    // 1. Try to read a fresh frame from the camera
			    // (This line will block until a frame arrives or times out)
			    if (cap.read(frame)) {
			        // 2. SUCCESS: The frame is valid, update the buffer immediately
			        buffer_.update(frame);
			    } 
			    else {
			        // 3. FAILURE: The camera driver or pipeline failed to deliver a frame
			        std::cout << "[Camera Thread] Failed to read frame or stream stopped." << std::endl;
			        
			        // Sleep briefly to prevent spinning the CPU at 100% if the driver hangs
			        std::this_thread::sleep_for(std::chrono::milliseconds(5));
			        
			        // Optional: break; if you want the thread to exit entirely when the camera dies
			    }
			}

          cap.release();
	}

	int sensor_id_;
	FrameBuffer& buffer_;
	std::atomic <bool> running_;
	std::thread worker_thread_;	
};

int main(){
	// create buffer
	FrameBuffer cam0_buffer;
	FrameBuffer cam1_buffer;

	// Create stream
	std::unique_ptr<ICameraStream> cam0 = std::make_unique<JetsonCameraStream>(0,cam0_buffer);
	std::unique_ptr<ICameraStream> cam1 = std::make_unique<JetsonCameraStream>(1,cam1_buffer);

	std::cout<<"Starting dual camera stream..."<<std::endl;
	cam0->start();
	cam1->start();

	cv::Mat view0, view1, combined;
	auto last_time = std::chrono::high_resolution_clock::now();
	int frame_count=0;

	// cv::namedWindow("Left Camera", cv::WINDOW_AUTOSIZE);
	// cv::namedWindow("Right Camera", cv::WINDOW_AUTOSIZE);
	// cv::setWindowProperty("Left Camera", cv::WND_PROP_TOPMOST, 1);
	// cv::setWindowProperty("Right Camera", cv::WND_PROP_TOPMOST, 1);
	
	// Display and performance 
	while(true){
		bool got0 = cam0_buffer.get(view0);
		bool got1 = cam1_buffer.get(view1);

		if(got0 && got1){
			 // Stack frames horizontally for a ultra-fast unified window layout
            // cv::hconcat(view0, view1, combined);
            // cv::imshow("Jetson J4012 Dual Camera Stream (60 FPS)", combined);

            // Profile Display Speed / FPS Counter
            frame_count++;
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = current_time - last_time;
            if (elapsed.count() >= 1.0) {
                std::cout << "Display Performance: " << frame_count / elapsed.count() << " FPS\r" << std::flush;
                frame_count = 0;
                last_time = current_time;
            }
		}
		
		if(cv::waitKey(1)==27){ break;} // break loop with ESC key 
	}

	std::cout<< "Capture complete, ending application."<<std::endl;
	return 0;
	
}
