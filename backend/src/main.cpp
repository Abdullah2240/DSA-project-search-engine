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

    // Define Route: /autocomplete?q=...&limit=10
    svr.Get("/autocomplete", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("q")) {
            std::string prefix = req.get_param_value("q");
            int limit = 10;
            if (req.has_param("limit")) {
                try {
                    limit = std::stoi(req.get_param_value("limit"));
                    if (limit < 1) limit = 1;
                    if (limit > 50) limit = 50; // Cap at 50 for performance
                } catch (...) {
                    limit = 10;
                }
            }
            std::string json_output = engine.autocomplete(prefix, limit);

            res.set_content(json_output, "application/json");
            res.set_header("Access-Control-Allow-Origin", "*"); // Fix CORS for React
        } else {
            res.status = 400;
            res.set_content("Missing 'q' param", "text/plain");
        }
    });

    std::cout << "Server starting on port 8080...\n";
    std::cout << "Go to";    
    std::cout << " http://localhost:8080/search?q=computer" << std::endl;
    if(!svr.listen("127.0.0.1", 8080)) {
        std::cerr << "Error: Could not start server\n";
    }
}