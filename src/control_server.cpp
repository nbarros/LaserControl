/*
 * control_server.cpp
 *
 *  Created on: 19 May 2023
 *      Author: nbarros
 */


// TO GET IT TO WORK:
// Copy the zm.hpp from https://github.com/zeromq/cppzmq into the include folder of zmq (3.2.4 at the time)
// VS needs to specify .lib files of platform! e.g.
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

int main () {
    //  Prepare our context and socket
    zmq::context_t context;
    zmq::socket_t socket(context,zmq::socket_type::rep);
    std::cout << "Binding socket" << std::endl;
    socket.bind ("tcp://*:5555");

    while (true) {
	std::cout << "Preparing to receive a request" << std::endl;
        zmq::message_t request;
	std::cout << "Message declared" << std::endl;

        //  Wait for next request from client
        socket.recv(request,zmq::recv_flags::none);
        std::cout << "Received something : [" << request.to_string() << "]" << std::endl;

        //  Do some 'work'
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    	std::cout << "Answering something" << std::endl;
    	// Pass by buffer
    	auto res = socket.send(zmq::str_buffer("hello world"), zmq::send_flags::none);
    }
    return 0;
}

