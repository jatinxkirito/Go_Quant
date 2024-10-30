#include<iostream>
#include "Server.h"
#include "auth.h"
using namespace std;


int main()
{
    DeribitAuth auth("6ynaYERD", "PxofNyS_r5RdXTAp2sGLL3DSQypM5qJewT5rW0bjJ3U");

    // Perform authentication
   auto success = auth.authenticate();


    if (!success.first) {
        std::cerr << "Authentication failed" << std::endl;
        return 1;
    }

   
    auto api = std::make_shared<DeribitAPI>(success.second);
   cout<< api->placeOrder("BTC-PERPETUAL", "buy", 1.0, 50000.0)<<endl;
   /* OrderbookServer server(api);
    server.run(9002);*/
    return 0;
}
//#define CURL_STATICLIB
//#include <iostream>
//#include "curl/curl.h"
//using namespace std;
//int main(int argc, char* argv[])
//{
//    CURL* req = curl_easy_init();
//    CURLcode res;
//    if (req)
//    {
//        curl_easy_setopt(req, CURLOPT_URL, "www.google.com");
//        curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1L);
//        res = curl_easy_perform(req);
//        if (res != CURLE_OK)
//        {
//            fprintf(stderr, "curl_easy_operation() failed : %s\n", curl_easy_strerror(res));
//        }
//    }
//    curl_easy_cleanup(req);
//    return 0;
//}