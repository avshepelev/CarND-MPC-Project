#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "MPC.h"
#include "json.hpp"

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.rfind("}]");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

// Evaluate a polynomial.
double polyeval(Eigen::VectorXd coeffs, double x) {
  double result = 0.0;
  for (int i = 0; i < coeffs.size(); i++) {
    result += coeffs[i] * pow(x, i);
  }
  return result;
}

// Fit a polynomial.
// Adapted from
// https://github.com/JuliaMath/Polynomials.jl/blob/master/src/Polynomials.jl#L676-L716
Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals,
                        int order) {
  assert(xvals.size() == yvals.size());
  assert(order >= 1 && order <= xvals.size() - 1);
  Eigen::MatrixXd A(xvals.size(), order + 1);

  for (int i = 0; i < xvals.size(); i++) {
    A(i, 0) = 1.0;
  }

  for (int j = 0; j < xvals.size(); j++) {
    for (int i = 0; i < order; i++) {
      A(j, i + 1) = A(j, i) * xvals(j);
    }
  }

  auto Q = A.householderQr();
  auto result = Q.solve(yvals);
  return result;
}

int main() {
  uWS::Hub h;

  // MPC is initialized here!
  MPC mpc;

  h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    string sdata = string(data).substr(0, length);
    cout << sdata << endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
      string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        string event = j[0].get<string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          vector<double> ptsx = j[1]["ptsx"]; // The global x positions of the waypoints
          vector<double> ptsy = j[1]["ptsy"]; // The global y positions of the waypoints
          double px = j[1]["x"]; // The global x position of the vehicle
          double py = j[1]["y"]; // The global y position of the vehicle
          double psi = j[1]["psi"]; // The orientation of the vehicle in radians converted from the Unity format to the standard format expected in most mathemetical functions 
          double v = j[1]["speed"];

          /*
          * TODO: Calculate steering angle and throttle using MPC.
          *
          * Both are in between [-1, 1].
          *
          */
          
          // The cross track error is calculated by evaluating at polynomial at x, f(x)
          // and subtracting y.
          // Number of waypoints in each dimension

          int size_pts = j[1]["ptsx"].size();
          Eigen::VectorXd ptsx_eigen(size_pts);
          Eigen::VectorXd ptsy_eigen(size_pts);

          // Transform waypoints to vehicle coordinates
          for (int i = 0; i < size_pts; i++)
          {
            ptsx_eigen[i] = (ptsx[i] - px) * cos(psi) + (ptsy[i] - py) * sin(psi);
            ptsy_eigen[i] = (ptsy[i] - py) * cos(psi) - (ptsx[i] - px) * sin(psi);
          }

          //std::cout << "ptsx_eigen \n" << ptsx_eigen << std::endl;
          //std::cout << "ptsx " << ptsx << std::endl;
          //std::cout << "px " << px << std::endl;
          //std::cout << "psi " << psi << std::endl;

          //std::cout << "ptsx_eigen" << ptsx_eigen << std::endl; 
          //std::cout << "ptsy_eigen" << ptsy_eigen << std::endl; 

          auto coeffs = polyfit(ptsx_eigen, ptsy_eigen, 3); 
          //std::cout << "coeffs" << coeffs << std::endl;
          double cte = polyeval(coeffs, 0.0);
          double epsi = - atan(coeffs[1]); //heading error TODO
          
          //std::cout << "here 2" << std::endl;

          Eigen::VectorXd state(6);
          state << 0.0, 0.0, 0.0, v, cte, epsi; // State is in vehicle coordinates

          double steer_value;
          double throttle_value;

          //std::cout << "here 2a" << std::endl;
          //std::cout << "state" << state <<std::endl;
          //std::cout << "coeffs" << coeffs << std::endl; 

          auto vars = mpc.Solve(state, coeffs);

          json msgJson; 

          // steering angle = - psi
          steer_value = -vars[0] / deg2rad(25); 
          throttle_value = vars[1];
                
          msgJson["steering_angle"] = steer_value;
          msgJson["throttle"] = throttle_value; 

          //double steer_value;
          //double throttle_value;

          //Display the waypoints/reference line
          vector<double> next_x_vals;
          vector<double> next_y_vals;

          for (int i = 0; i < ptsx.size(); i++) {
            next_x_vals.push_back((ptsx[i] - px) * cos(psi) + (ptsy[i] - py) * sin(psi));
            next_y_vals.push_back((ptsy[i] - py) * cos(psi) - (ptsx[i] - px) * sin(psi));
          }

          //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
          // the points in the simulator are connected by a Yellow line

          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          // NOTE: Remember to divide by deg2rad(25) before you send the steering value back.
          // Otherwise the values will be in between [-deg2rad(25), deg2rad(25] instead of [-1, 1].
          
          //Display the MPC predicted trajectory 
          vector<double> mpc_x_vals;
          vector<double> mpc_y_vals;   

          //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
          // the points in the simulator are connected by a Green line
          auto N = (vars.size() - 2)/ 2;

          for (int i=2; i < N+2; i++) {
            mpc_x_vals.push_back(vars[i]);
            mpc_y_vals.push_back(vars[i + N]);
          }
 
          msgJson["mpc_x"] = mpc_x_vals;
          msgJson["mpc_y"] = mpc_y_vals;
 
          //std::cout << "here 6" << std::endl;

          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          std::cout << msg << std::endl;
          // Latency
          // The purpose is to mimic real driving conditions where
          // the car does actuate the commands instantly.
          //
          // Feel free to play around with this value but should be to drive
          // around the track with 100ms latency.
          //
          // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE
          // SUBMITTING.
          this_thread::sleep_for(chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
