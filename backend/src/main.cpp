#include "httplib.h" 
#include "../include/SearchService.hpp"
#include <iostream>

int main() {
    SearchService engine;
    httplib::Server svr;

    // Define Route: /search?q=...
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("q")) {
            std::string query = req.get_param_value("q");
            std::string json_output = engine.search(query);

            res.set_content(json_output, "application/json");
            res.set_header("Access-Control-Allow-Origin", "*"); // Fix CORS for React
        } else {
            res.status = 400;
            res.set_content("Missing 'q' param", "text/plain");
        }
    });

    std::cout << "Server starting on port 8080...\n";
    std::cout << "Go to";    
    std::cout << " http://localhost:8080/search?q=computer";
    if(!svr.listen("127.0.0.1", 8080)) {
        std::cerr << "Error: Could not start server\n";
    }
}