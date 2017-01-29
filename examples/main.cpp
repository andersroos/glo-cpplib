
#include <atomic>
#include <memory>
#include <glo.hpp>

//
// Simple program to use  during development.
//

using namespace std;
using namespace glo;

int main()
{
   auto t1 = 50s;
   auto t2 = t1 / 100;
   
   std::cerr << t2.count() << std::endl;
}
