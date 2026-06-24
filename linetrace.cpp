char buffer[20];
unsigned char rx_buffer[256];
bool silverDetected = false;

#include <termios.h>                                                         
#include <stdio.h>
#include <stdlib.h>	
#include <string.h>                                                       
#include <fcntl.h>                                                                                                               
#include <sys/types.h> 
#include <stdint.h>
#include <sys/signal.h>
#include <time.h>
#include <stdbool.h>	
#include <errno.h>
#include <lccv.hpp>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <unistd.h>
//#include <libcamera/libcamera.h>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

#define BAUDRATE B9600                                                    
#define MODEMDEVICE "/dev/ttyAMA0"

bool objCheck = false;

int uart0_filestream = -1;

void init(){
	//-------------------------
	//----- SETUP USART 0 -----
	//-------------------------
	//At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively

	
	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
	uart0_filestream = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
		exit(-1);
	}
	
	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	struct termios options;
	tcgetattr(uart0_filestream, &options);
	options.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(uart0_filestream, TCIFLUSH);
	tcsetattr(uart0_filestream, TCSANOW, &options);
	
}

void move(int lspeed, int rspeed, int type = 0) {
	sprintf(buffer, "[%d,%d]", lspeed, rspeed);
	std::cout << "BUFFER: " << buffer << std::endl;
	write(uart0_filestream, &buffer[0], strlen(buffer));
}

void readBuffer() {
	if(uart0_filestream != -1) {
		int rx_length = 0;
		
		rx_length = read(uart0_filestream, rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
		rx_buffer[rx_length] = '\0';
		
		if (rx_length < 0) {
			//An error occured (will occur if there are no bytes)
			std::cout << " rx_len < 0 error!" << std::endl;
		}
		else if (rx_length == 0) {
			//No data waiting
			std::cout << "rx_len == 0 no data" << std::endl;
		}
	}
}

void findLine(cv::Mat input_img, cv::Mat output_img) { //Takes in the video feed and thresholds it
	
	cv::Mat img_copy = input_img.clone();
	
	cv::cvtColor(img_copy, img_copy, cv::COLOR_BGR2GRAY);
	cv::cvtColor(output_img, output_img, cv::COLOR_BGR2GRAY);
	cv::medianBlur(img_copy, img_copy, 5);
	threshold(img_copy, output_img, 40, 255, cv::THRESH_BINARY_INV);
}

bool contour_cmp(std::vector<cv::Point> &a, std::vector<cv::Point> &b) {
	return contourArea(a) > contourArea(b);
}

void clear_buffer()	{
	memset(rx_buffer, 0, sizeof(rx_buffer));
	//uart0_filestream.clear();
	
}

void detectSilver(cv::Mat img){
	std::vector<std::vector<cv::Point>> Contours;
	cv::Mat hsvimg, grayimg;
	cv::Rect r;
	int noise = 0;
	int allContours = 0;
	cv::cvtColor(img, grayimg, cv::COLOR_BGR2GRAY);
	cv::threshold(grayimg, grayimg, 127, 255, cv::THRESH_BINARY_INV);
	cv::cvtColor(img, hsvimg, cv::COLOR_BGR2HSV);
	
	//low h s v - high h s v
	inRange(hsvimg, cv::Scalar(0, 0, 150), cv::Scalar(1, 1, 255), hsvimg);
	
	findContours(hsvimg, Contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	
	for(auto& cnt : Contours) {
			int area = contourArea(cnt);
				
			if(area < 300) {
				noise += 1;
			}	
			allContours += 1;
		}
		
	if(allContours > noise) {
		std::cout << "SILVER DETECTED!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		move(5, 5);
		std::this_thread::sleep_for(100ms);
		move(1, 1);
		std::this_thread::sleep_for(8000ms);
	}
	else {
	}
}

void greensquare(cv::Mat img) {
	//std::cout << "Finding greensquare. " << std::endl;
	std::vector<std::vector<cv::Point>> Contours;
	cv::Mat hsvimg, grayimg;
	cv::Rect r;
	bool leftgreen = false, rightgreen = false, doublegreen = false;
	
	//std::cout << "about to convert color:" << std::endl;
	//img = img(cv::Rect(0, 40, 320, 200));
	cv::cvtColor(img, grayimg, cv::COLOR_BGR2GRAY);
	cv::threshold(grayimg, grayimg, 127, 255, cv::THRESH_BINARY_INV);
	//std::cout << "grayimg" << std::endl;
	cv::cvtColor(img, hsvimg, cv::COLOR_BGR2HSV);
	//std::cout << "color converted." << std::endl;
	
	//low h s v - high h s v
	inRange(hsvimg, cv::Scalar(0, 100, 50), cv::Scalar(180, 255, 245), hsvimg); 
	//0 110 50 170 255 195
	//SR 2026 - 0, 107, 70  - 98, 255, 195
	
	
	//43, 100, 50 - 94, 255, 255 : home
	//inRange(hsvimg, cv::Scalar(0, 110, 101), cv::Scalar(180, 235, 205), hsvimg);
	//std::cout << "thresholded" << std::endl;
	
	findContours(hsvimg, Contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	//std::cout << "Contours found." << std::endl;
	int gscount = 0;
	int adicount = 0;
	while(1) {
		for(auto& cnt : Contours) {
				//std::cout << "Searching greensquare contours" << std::endl;
				
				int area = contourArea(cnt);
					
				if(area < 150) {
					continue;
				}
				r = boundingRect(cnt);
					
				cv::Point center(r.x + (r.width/2), r.y + (r.height/2));
				//120, 50, 270                //200 before
				
				std::cout << "center " << center.y << std::endl;
				
				if(center.y > 120/*|| center.x < 20 || center.x > 300*/ && adicount == 0) {  //ignore green squares around edge
					continue;
				} else if (adicount == 0) {
					adicount = 1;
					break;
				}
				
				gscount++;
				std::cout << "COUNTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT: " << gscount << std::endl;
				
				cv::Point left(r.x - 5, r.y + (r.height / 2)), right(r.x + r.width + 5, r.y + (r.height /2 )), bottom(r.x + (r.width / 2), r.y + r.height + 5), top(r.x + (r.width / 2), r.y - 5);
				
				cv::circle(img, center, 5, cv::Scalar(255, 0, 0), -1);
				cv::circle(img, bottom, 5, cv::Scalar(0, 255, 0), -1);
				cv::circle(img, top, 5, cv::Scalar(255, 255, 0), -1);
				cv::circle(img, right, 5, cv::Scalar(255, 0, 255), -1);
				cv::circle(img, left, 5, cv::Scalar(0, 255, 255), -1);
				
				if(bottom.y > 230) {
					move(20, 20);
					std::this_thread::sleep_for(200ms);
							
					move(0, 0);
					std::this_thread::sleep_for(1000ms);
				}
				
				std::cout << "Circles drawn" << std::endl;
				
				std::cout << "topbottom" << (int)grayimg.at<uchar>(top) << "," << (int)grayimg.at<uchar>(bottom);
				std::cout << "leftRight" << (int)grayimg.at<uchar>(left) << "," << (int)grayimg.at<uchar>(right);
				
				
				
				//255=white, 0 =black
				if(grayimg.at<uchar>(bottom) == 0 && grayimg.at<uchar>(top) == 255) {
					putText(img, "False Green", cv::Point(5, 30) , cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));
					std::cout << "False Green" << std::endl;
				}
				else if(grayimg.at<uchar>(top) == 0 && grayimg.at<uchar>(bottom) == 255) {
					/*move(0, 0);
					std::this_thread::sleep_for(1000ms);*/
					if(grayimg.at<uchar>(right) == 0) {
						leftgreen = true;
						if(rightgreen) {
							doublegreen = true;
						}
						move(10, 20);
						std::this_thread::sleep_for(100ms);
						
						move(0, 0);
						std::this_thread::sleep_for(1000ms);
						
						/*if(grayimg.at<uchar>(left) == 0) {
						rightgreen = true;
						}*/
					}
					
					move(0, 0);
					std::this_thread::sleep_for(500ms);
					
					if(grayimg.at<uchar>(left) == 0) {
						rightgreen = true;
						if(leftgreen) {
							doublegreen = true;
						}
						
						move(20, 10);
						std::this_thread::sleep_for(100ms);	
						
						move(0, 0);
						std::this_thread::sleep_for(1000ms);
						
						/*if(grayimg.at<uchar>(right) == 0) {
						leftgreen = true;
						}*/
					}
				}
			}
			
			
			if (adicount == 1) {
				adicount++;
				continue;
			}
			
			break;
		}
		
		//hardcoding values and delays for the different turns
		if(doublegreen) {
			putText(img, "Double Green", cv::Point(5, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));
			std::cout << "Double Green" << std::endl;
			move(0,0);
			std::this_thread::sleep_for(1000ms);
			
			move(50, -50);
			std::this_thread::sleep_for(800ms);
		}
		else if (leftgreen) {
			putText(img, "Left Turn", cv::Point(5, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));
			std::cout << "Left Green" << std::endl;
			/*move(0,0);
			std::this_thread::sleep_for(1000ms);*/
			
			move(25, 25);
			std::this_thread::sleep_for(550ms);//450
			
			move(-25, 25);
			std::this_thread::sleep_for(750ms);
			
			move(0,0);
			std::this_thread::sleep_for(1500ms);
		}
		else if(rightgreen) {
			putText(img, "Right Turn", cv::Point(5, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));
			std::cout << "Right Green" << std::endl;
			/*move(0,0);
			std::this_thread::sleep_for(1000ms);*/
			
			move(25, 25);
			std::this_thread::sleep_for(550ms);
			
			move(25, -25);
			std::this_thread::sleep_for(750ms);
			
			move(0,0);
			std::this_thread::sleep_for(1500ms);
		}	
		else{
			std::cout << "None!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		}
						
		leftgreen = rightgreen = false;
		
		imshow("greensquare", img);	
			
}

bool detectRed(cv::Mat img) {
	std::vector<std::vector<cv::Point>> Contours;
	cv::Mat hsvimg, grayimg;
	cv::Rect r;
	int noise = 0;
	int allContours = 0;
	cv::cvtColor(img, grayimg, cv::COLOR_BGR2GRAY);
	cv::threshold(grayimg, grayimg, 127, 255, cv::THRESH_BINARY_INV);
	cv::cvtColor(img, hsvimg, cv::COLOR_BGR2HSV);
	    
	//low h s v - high h s v
	inRange(hsvimg, cv::Scalar(0, 195, 110), cv::Scalar(180, 245, 235), hsvimg);
	
	findContours(hsvimg, Contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	
	for(auto& cnt : Contours) {
			int area = contourArea(cnt);
				
			if(area < 15000) {
				noise += 1;
			}	
			allContours += 1;
		}
		
	if(allContours > noise) {
		std::cout << "RED DETECTED!!!!!!!!!!!!!!!!!!!!!" << std::endl;
		return true;
	}
	else {
		return false;
	}
}

void linetrace(cv::Mat img, std::vector<std::vector<cv::Point>> Contour) {
	
}

int printtest(){
	while(true){
		char buffer[15] = "hi\n";
		write(uart0_filestream, &buffer[0], 3);
		sleep(1);
		int readBytes = read(uart0_filestream, buffer, 3);
		for (int i=0;i<readBytes;i++)
			printf("%c",buffer[i]);
	}
}

void obstacle() {
	int dist = 0;
	int count = 0;
	
	for(int digit = 1; rx_buffer[count] != ',' && rx_buffer[count] != '\0'; count++, digit*=10) {
		dist *= digit;
		dist += rx_buffer[count] - '0';
	}
		
			
	std::cout << "Distance: " << dist << " cm" << std::endl;
	
			if(objCheck == true && dist > 8) {
				objCheck = false;
			}
			
			if(dist <= 8 && dist > 0 && objCheck == false) {                       //OBSTACLE AVOIDANCE HERE
				std::cout << "OBJ!" << std::endl;
				float robotDiam = 16;
				int objDiam = 5;
				float outDiam = objDiam + 2*(robotDiam + dist);
				float inDiam = objDiam + robotDiam*2; 
				float inSpd = 10;
				float outSpd = (outDiam*inSpd) / inDiam;
				
				//move around obj - change delay times
				move(1,1);
				std::this_thread::sleep_for(100ms);
				
				move(-25, 25);
				std::this_thread::sleep_for(900ms);
				
				move(35, 18);
				std::this_thread::sleep_for(4300ms);
				
				move(-25, 25); 
				std::this_thread::sleep_for(900ms);
				
				objCheck = true;
				//clear_buffer();
			}
}

bool sortByContourArea(std::vector<cv::Point> contour1, std::vector<cv::Point> contour2){
		double a1 = cv::contourArea(contour1);
		double a2 = cv::contourArea(contour2);
		return a1 < a2;
}

double distance_between_points_manual(const cv::Point& p1, const cv::Point& p2) {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
	
	
	init();
	
	char read_buffer[3];
	if(uart0_filestream != -1) {
		int readBytes = read(uart0_filestream, read_buffer, 3);
	
		while(readBytes <= 0) {
			readBytes = read(uart0_filestream, read_buffer, 3);
			std::cout << "waiting for cytron to start... " << std::endl;
			sleep(1);
		}
	}
	else {
		std::cout << "uart0_filestream = -1" << std::endl;
		while(1);
	}
	std::cout << "Starting..." << std::endl;
	
	
	char input;
	std::vector<std::vector<cv::Point>> Contours, middleCont;
	bool stopmotors = false;
	float lspeed, rspeed;
	cv::Mat middle;
	
	lccv::PiCamera cam1;                            //cam setup
	cam1.options->camera = 0;
	cam1.options->video_width =320;
	cam1.options->video_height=240;
	cam1.options->framerate= 30;
	cam1.options->verbose=true;

	cam1.startVideo();
	namedWindow("Video", cv::WINDOW_NORMAL);
	
	cv::Mat img, slice;
	while(true) {
		if(!cam1.getVideoFrame(img, 99999999)) {
			std::cout << "camera fail!!!!!!!!!!!!!!!!!!!!" << std::endl;
			break;
		}
		
		//imshow("Video", img);
		cv::Mat img_cont = img.clone();
		
		cv::Mat line = img.clone();
		cv::cvtColor(line, line, cv::COLOR_BGR2GRAY);
		cv::medianBlur(line, line, 5);
		cv::threshold(line, line, 80, 255, cv::THRESH_BINARY_INV);
		//imshow("thresh", line);
		line = line(cv::Rect(0, 20, 320, 220));
		img_cont = img_cont(cv::Rect(0, 20, 320, 220));
		cv::findContours(line, Contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
		
		if(Contours.size() <= 0) {
			//std::cout << "no countours" << std::endl;
			move(20, 20);
			continue;
		}
		
		cv::drawContours(img_cont, Contours, -1, cv::Scalar(255, 0, 0), 2);
		
		std::sort(Contours.begin(), Contours.end(), contour_cmp);
		 
		cv::Moments mu = moments(Contours[0]);
		   
		cv::Point centroid;
		
		centroid.x = mu.m10 / mu.m00;
		centroid.y = mu.m01 / mu.m00;
		
		cv::circle(img_cont, centroid, 6, cv::Scalar(0, 0, 255), 5);
		
		float targetVal = 160; //Center of the screen; 160
		float kp = 0.3;//0.35
		
		 //215 
		if(centroid.y < 30) {//may have to change value - panic kp
			kp = 0.5;
			std::cout << "    !!!!!!!!!!    PANIC" << std::endl;
		}
		else if(centroid.y < 80) {
			kp = 0.4;
		}
		
		//kp=0.00456*(220.0-centroid.y)-0.1083;
				
		float error = targetVal-centroid.x;                 
		float speedChange = error * kp;
		
		float startSpeed = 25;                        //PID
		lspeed = startSpeed + speedChange;
		rspeed = startSpeed - speedChange;
		
		std::cout << "Lspeed: " << lspeed << "  Rspeed: " << rspeed << std::endl;
		
		std::cout << "error: " << error << "   speedchange: " << speedChange << std::endl;
		
		if(uart0_filestream != -1) {
			//std::cout << "uart filestream working" << std::endl;
			if(stopmotors) {
				std::cout << "stopped" << std::endl;
				move(0,0);
			}
			else {              
				float CAP = 99;                         //cap the speed
				if(lspeed > 100) {
					lspeed = CAP;
				}
				else if(lspeed < -100) {
					lspeed = -CAP;
				}
				if(rspeed > 100) {
					rspeed = CAP;
				}
				else if(rspeed < -100) {
					rspeed = -CAP;
				}
				
				move((int)lspeed, (int)rspeed);
				}
		}
		else {
			std::cout << "FILESTREAM NOT WORKING" << std::endl;
		}
		
		//READ ALL INTO 1 BUFFER IN MAIN	
		readBuffer();
		obstacle();
		
		//detectSilver(img);
		
		/*if(detectRed(img)) {
			move(1, 1);
			move(0,0);
			std::this_thread::sleep_for(10000ms);
		}*/
		
		//obstacle();
		
		greensquare(img);		/////////////////////////////////////////////////////////////////////////////////////////////////////GREEN SQUARE!!!
		
		std::cout << rx_buffer << std::endl;
			
		std::this_thread::sleep_for(10ms); //stop buffer overflow
		
		//imshow("Line", line);
		imshow("contours", img_cont);
		
		input = (char)cv::waitKey(1);
		if(input == 'q') {
			break;
		}
		else if(input == ' ') {
			stopmotors = true;
		}
		else if(input == 'g') {
			stopmotors = false;
			silverDetected = false;
		}
	}
	
	cam1.stopVideo();
	cv::destroyAllWindows();
}
