#include </include/HttpController.hpp>
#include "../models/:some data structure i make and then remove the colons:"

using namespace drogon;

class SearchController : public drogon::HttpController<SearchController> {
    private:
        SearchEngine engine; // Whatever our model is named
        SearchServices search;
    
    public:

        METHOD_LIST_BEGIN

        ADD_METHOD_TO(SeachController::search, /search, Get);

        METHOD_LIST_END

        void search(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> && callback)
}