/** Dependency-free runner for the exact same behavioral cases as GoogleTest. */
#include "CoreCases.h"
#include <iostream>
int main(){int failures=0;for(const auto& [name,run]:tests::cases()){try{run();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}}std::cout<<tests::cases().size()<<" cases, "<<failures<<" failures\n";return failures?1:0;}
