
#include "allocator.h"
#include <iostream>
static int counter{ 0 };
class Person {
private:
   
    std::string name;
    int age;

public:
    Person(std::string_view n, int a) : name{ n }, age{ a } {
        counter++;
        std::cout << name<<" Created" << '\n';
    };
    ~Person() {
        counter--;
        std::cout << name << " Destroyed"<<'\n'; 
    }
 };

int main() {
    try {
        PoolAllocator<Person> x(4);
        Person* test = x.create("John", 25);
        x.destroy(test);

    }
    catch (const std::exception& e) {
        std::cerr << "Allocator error: " << e.what() << '\n';
    }


    return 0;
}
