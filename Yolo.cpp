/*
        V1 -- Only 1 set of victims (dead)
        6/25
        


*/


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
#include <lccv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/ocl.hpp>


//yolo defenitions
const float SCORE_THRESHOLD = 0.90; 
const float NMS_THRESHOLD = 0.35;


bool is_cuda = false;




cv::Size2f imageShape(cv::Size(320,320));


std::vector<std::string> class_list{"dv", "dz", "av", "az"};


using namespace std::chrono_literals;




using namespace cv;
using namespace std;


#define BAUDRATE B9600                                                    
#define MODEMDEVICE "/dev/ttyAMA0"


char buffer[20];
unsigned char rx_buffer[256];
bool silverDetected = false;
bool objCheck = false;


int uart0_filestream = -1;


vector<float> class_ids;
vector<float> confidences;
vector<Rect> boxes;
vector<int> nms_result;
vector<float> box_dist;




struct object {
        int class_id;
        int dist;
};


void init(){
        //-------------------------
        //----- SETUP USART 0 -----
        //-------------------------
        //At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively


        
        //OPEN THE UART
        //The flags (defined in fcntl.h):
        //        Access modes (use 1 of these):
        //                O_RDONLY - Open for reading only.
        //                O_RDWR - Open for reading and writing.
        //                O_WRONLY - Open for writing only.
        //
        //        O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
        //                                                                                        if there is no input immediately available (instead of blocking). Likewise, write requests can also return
        //                                                                                        immediately with a failure status if the output can't be written immediately.
        //
        //        O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
        uart0_filestream = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NDELAY);                //Open in non blocking read/write mode
        if (uart0_filestream == -1)
        {
                //ERROR - CAN'T OPEN SERIAL PORT
                printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
                exit(-1);
        }
        
        //CONFIGURE THE UART
        //The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
        //        Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
        //        CSIZE:- CS5, CS6, CS7, CS8
        //        CLOCAL - Ignore modem status lines
        //        CREAD - Enable receiver
        //        IGNPAR = Ignore characters with parity errors
        //        ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
        //        PARENB - Parity enable
        //        PARODD - Odd parity (else even)
        struct termios options;
        tcgetattr(uart0_filestream, &options);
        options.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;                //<Set baud rate
        options.c_iflag = IGNPAR;
        options.c_oflag = 0;
        options.c_lflag = 0;
        tcflush(uart0_filestream, TCIFLUSH);
        tcsetattr(uart0_filestream, TCSANOW, &options);
        
}


void move(int lspeed, int rspeed, int type = 1) {
        sprintf(buffer, "[%d,%d,%d]", type, lspeed, rspeed);
        std::cout << "BUFFER: " << buffer << std::endl;
        write(uart0_filestream, &buffer[0], strlen(buffer));
}


void readBuffer() {
        if(uart0_filestream != -1) {
                int rx_length = 0;
                
                rx_length = read(uart0_filestream, rx_buffer, 255);                //Filestream, buffer to store in, number of bytes to read (max)
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


void clear_buffer()        {
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
        bool leftgreen = false, rightgreen = false;
        
        //std::cout << "about to convert color:" << std::endl;
        //img = img(cv::Rect(0, 40, 320, 200));
        cv::cvtColor(img, grayimg, cv::COLOR_BGR2GRAY);
        cv::threshold(grayimg, grayimg, 127, 255, cv::THRESH_BINARY_INV);
        //std::cout << "grayimg" << std::endl;
        cv::cvtColor(img, hsvimg, cv::COLOR_BGR2HSV);
        //std::cout << "color converted." << std::endl;
        
        //low h s v - high h s v
        inRange(hsvimg, cv::Scalar(0, 107, 70), cv::Scalar(98, 255, 195), hsvimg); //43, 100, 50 - 94, 255, 255 : home
        //inRange(hsvimg, cv::Scalar(0, 110, 101), cv::Scalar(180, 235, 205), hsvimg);
        //std::cout << "thresholded" << std::endl;
        
        findContours(hsvimg, Contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        //std::cout << "Contours found." << std::endl;


        for(auto& cnt : Contours) {
                        //std::cout << "Searching greensquare contours" << std::endl;
                
                        int area = contourArea(cnt);
                                
                        if(area < 300) {
                                continue;
                        }
                        r = boundingRect(cnt);
                                
                        cv::Point center(r.x + (r.width/2), r.y + (r.height/2));
                        //120, 50, 270                //200 before
                        if(center.y < 50 || center.y > 180 /*|| center.x < 20 || center.x > 300*/) {  //ignore green squares around edge
                                continue;
                        }
                        
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
                                        
                                        move(10, 20);
                                        std::this_thread::sleep_for(100ms);
                                        
                                        move(0, 0);
                                        std::this_thread::sleep_for(1000ms);
                                        
                                        if(grayimg.at<uchar>(left) == 0) {
                                        rightgreen = true;
                                        }
                                }
                                
                                /*move(0, 0);
                                std::this_thread::sleep_for(1000ms);*/
                                
                                if(grayimg.at<uchar>(left) == 0) {
                                        rightgreen = true;
                                        
                                        move(20, 10);
                                        std::this_thread::sleep_for(100ms);        
                                        
                                        move(0, 0);
                                        std::this_thread::sleep_for(1000ms);
                                        
                                        if(grayimg.at<uchar>(right) == 0) {
                                        leftgreen = true;
                                        }
                                }
                        }
                }
                
                //hardcoding values and delays for the different turns
                if(leftgreen && rightgreen) {
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
                
                //imshow("greensquare", img);                
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
                                
                                move(35, 10);
                                std::this_thread::sleep_for(4300ms);
                                
                                move(-25, 25);
                                std::this_thread::sleep_for(1000ms);
                                
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






void yolo(cv::dnn::Net net, Mat img){
                //blur( img, img, Size( 3, 3 ), Point(-1,-1) );
                
                //resize(img, img, Size(320,240), 0, 0, INTER_LINEAR);
                
                
                Mat blob;
                
                class_ids.clear();
                confidences.clear();
                boxes.clear();
                nms_result.clear();
                box_dist.clear();
                
                
                // does necessary scaling and stuff for NN usage
                dnn::blobFromImage(img, blob, 1.0/255, imageShape, Scalar(), true, false);
                //dnn::blobFromImage(img, blob, 1.0, imageShape);//, Scalar(), true, false);
                //dnn::blobFromImage(img, blob, 1.0/255, imageShape, Scalar(0,0,0), false, false, CV_32F);
                
                net.setInput(blob);
                
                
                vector<Mat> outputs;
                net.forward(outputs, net.getUnconnectedOutLayersNames());
                
                
                // some info about the output
                //int rows = outputs[0].size[2];
                //int dimensions = outputs[0].size[1];
                int rows = outputs[0].size[1];
                int dimensions = outputs[0].size[2];
                if(dimensions > rows){
                        printf("\n\nYOLO 8\n\n");
                        
                }
                else{
                        printf("\n\nYOLO 5\n\n");
                }
                //printf("Rows: %5d\tDimensions: %5d\n", rows, dimensions);     // 2100 rows and 8 dimensions
                
                // swap for yolov8 
                rows = outputs[0].size[2];
                dimensions = outputs[0].size[1];
                
                // changing around dimensions (it changed for no reason in yolov8 so we need to switch it back)
                outputs[0] = outputs[0].reshape(1, dimensions);
                cv::transpose(outputs[0], outputs[0]);
                
                float *data = (float *)outputs[0].data;
                
                 
                
                
                
                //float x_factor = img.cols / imageShape.width;
                //float y_factor = img.rows / imageShape.height;
                
                
                // loop through all the rows in the results
                
                
                for(int i = 0; i < rows; i++){
                        float *classes_scores = data+4;
                        
                        
                        
                        Mat scores(1, class_list.size(), CV_32FC1, classes_scores);
                        Point class_id;
                        double maxClassScore;
                        
                        
                        minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);
                        
                        
                        
                        if(maxClassScore > 0.5){//SCORE_THRESHOLD){
                                printf("i = %d\tscore: %.2f\n", i, maxClassScore);
                                
                                confidences.push_back(maxClassScore);
                                class_ids.push_back(class_id.x);
                                
                                printf("class id: %d\n", class_id.x);
                                
                                float x = data[0];
                                float y = data[1];
                                float w = data[2];
                                float h = data[3];
                                
                                //printf("x,y => %.2f, %.2f\n", x, y);
                                
                                
                                
                                
                                int left = x * img.cols - w * img.cols / 2;
                                int top = y * img.rows - h * img.rows / 2;
                                
                                int width = int(w * img.cols);
                                int height = int(h * img.rows);
                                
                                //int left = int((x - 0.5 * w) * x_factor);
                                //int top = int((y - 0.5 * h) * y_factor);
                                
                                //int width = int(w * x_factor);
                //int height = int(h * y_factor);




                                boxes.push_back(Rect(left, top, width, height));
                                //rectangle(img, Rect(left, top, width, height), Scalar(0,255,0), 1);
                        }
                        
                        
                        data += dimensions;
                }
                
                //done with checking
                
                printf("does it make it this far?????");
                
                // all done, time to draw + nms boxes
                dnn::NMSBoxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD, nms_result);
                
                
                for(unsigned long i = 0; i < nms_result.size(); i++){
                        int idx = nms_result[i];
                        rectangle(img, boxes[idx], Scalar(0,255,0), 1); 
                        if (class_ids[idx] == 0 || class_ids[idx] == 2)
                                box_dist.push_back(1.6 * 265 / boxes[idx].width); //height in 1 * focal / 150 in pixels = 1[=q
                        else 
                                box_dist.push_back(3 * 265 / boxes[idx].height); // zones
                        cout << "\n" << "dist" << box_dist[0] << endl;
                        putText(img, to_string( int(confidences[idx] * 100)) + "% " + class_list[class_ids[idx]], Point(boxes[idx].x, boxes[idx].y), 1, 3, Scalar(0,255,0), 2);
                }
                /*
                
                if (nms_result.size() > 0) {
                        cout << "wassap" <<endl;
                        move(0, 0);
                        for (unsigned long i = 0; i < nms_result.size(); i++) {
                                int idx = nms_result[i];
                                Rect obj = boxes[idx];
                                if(class_id[idx] == 2) {
                                        int centerX = obj.x + (obj.width / 2);
                                        int centerY = obj.y + (obj.length / 2);
                                        if(centerX > 
                                }
                        }
                }
                else {
                        move(30, -30);
                }*/
                
                //setup for passing into return. prob bad but i aint setting up a struct
                //reverse(box_dist.begin(), box_dist.end());
                        
                //vector<vector<float>> returnbox;
                
                //returnbox.push_back(class_ids);
                //returnbox.push_back(box_dist*/
                
                
                                
                imshow("full", img);
                
}


int main() {
        init();
        
        cv::dnn::Net net = cv::dnn::readNet("/home/pi/Documents/adi/RCJ_JAAVAA2026/adi(working).onnx");
                
        if(is_cuda){
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        }
        else{ 
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
        
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
        
        bool holding_ball = false;
        int counter = 0;
        
        while(true) {
                
                if(!cam1.getVideoFrame(img, 99999999)) {
                        std::cout << "camera fail!!!!!!!!!!!!!!!!!!!!" << std::endl;
                        break;
                }
                
                flip(img, img ,0);
                
                Mat origin = img;
                
                imshow("Video", img);

                yolo(net, img);
                
                if (nms_result.size() > 0) { //if anything in view
                        for (unsigned long i = 0; i < nms_result.size(); i++) { //check through results vector to see what objects are in view
                                int idx = nms_result[i];
                                Rect obj = boxes[idx];
                                
                                cout << class_ids[idx] << " THIS2" << endl;
                                
                                if(class_ids[idx] == 2 && !holding_ball) //dead victim seen and not holding ball
                                {
                                        int centerX = obj.x + (obj.width / 2); //center of ball
                                        int center = img.cols / 2; //center of screen
                                        cout << centerX << " THIS IS IMPORTANT" << endl;
                                        
                                        circle(img, Point(centerX, obj.y + obj.height / 2), 8, Scalar(0, 0, 255), -1); //draws centere
                                        circle(img, Point(center - 50, obj.y), 8, Scalar(0, 0, 255), -1); //draws left barrier
                                        circle(img, Point(center + 50, obj.y), 8, Scalar(0, 0, 255), -1); //draws right barrier
                                        cout << "dist" << box_dist[i] << endl;;
                                        if (centerX > center - 50 && centerX < center + 50) { //if in center, go forward
                                                cout << "CENTERED" << endl;
                                                if (box_dist[i] > 3) { //move forward
                                                        if (centerX > center) {
                                                                move(20, 32);
                                                        } 
                                                        else if(centerX == center) {
                                                                move(25, 25);
                                                        }
                                                        else {
                                                                move (32, 20);
                                                        }
                                                        cout << "CENTERED" << endl;
                                                        break;
                                                }
                                                else { // if close. grab ball
                                                        move(0, 0, 2);
                                                        cout << "here" << endl;
                                                        
                                                        yolo(net, origin); 
                                                        holding_ball = true;
                                                        
                                                        counter++;
                                                        break;
                                                }
                                        }
                                        
                                } 
                                else if (holding_ball && counter == 1) { //dead zone seen and holding ball
                                        cout << holding_ball << endl;
                                        int idx = nms_result[i];
                                        Rect obj = boxes[idx];
                                        
                                        cout << class_ids[idx] << " we looking for da box" << endl;
                                        
                                        if(class_ids[idx] == 3) {
                                                int centerX = obj.x + (obj.width / 2);
                                                int center = img.cols / 2;
                                                cout << centerX << "         THIS IS zone" << endl;
                                                
                                                circle(img, Point(centerX, obj.y + obj.height / 2), 8, Scalar(0, 0, 255), -1);
                                                circle(img, Point(center - 20, obj.y), 8, Scalar(0, 0, 255), -1);
                                                circle(img, Point(center + 20, obj.y), 8, Scalar(0, 0, 255), -1);
                                                cout << "dist" << box_dist[i] << endl;;
                                                if (centerX > center - 50 && centerX < center - 30) {
                                                        cout << "centered" << endl;
                                                        if (box_dist[i] > 3) 
                                                        {
                                                                move(30, 30);
                                                                cout << "going" << endl;
                                                                break;
                                                        }
                                                        else 
                                                        {
                                                                move(40,-40);
                                                                cout << "here" << endl;
                                                                yolo(net, origin); 
                                                                holding_ball = false;
                                                                
                                                                counter++;
                                                                break;
                                                        }
                                                }
                                        }
                                }
                                else {
                                        move(-20, 20);
                                }
                        }
                } else 
                {
                        move(-20, 20);
                }
                
                                
                        
                
                //READ ALL INTO 1 BUFFER IN MAIN        
                readBuffer();
                
                //std::this_thread::sleep_for(10ms);
                
                //imshow("cont", img_cont);
                imshow("line", img);
                        
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
