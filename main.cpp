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

    
    
    auto api = std::make_shared<DeribitAPI>(success.second);//access token for the session
   /*cout<< api->placeOrder("ETH-PERPETUAL","buy",5,0,"market") << endl;
   cout<<api->placeOrder("ETH-PERPETUAL","sell", 5,0,"market")<<endl;*/
   /*cout << api->placeOrder("ETH-PERPETUAL", "buy", 10, 2600) << endl;*/
   /*cout << api->modifyOrder("ETH-14472452183", 2621, 8) << endl;*/
  /* cout << api->cancelOrder("ETH-14472452183");*/
 
    OrderbookServer server(api);
    server.run(9002);
  
   
}
