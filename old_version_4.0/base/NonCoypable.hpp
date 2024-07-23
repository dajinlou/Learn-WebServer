#ifndef NONCOYPABLE_H_
#define NONCOYPABLE_H_

class NonCoypable
{

protected:
    NonCoypable(/* args */){}
    ~NonCoypable(){}
private:
    NonCoypable(const NonCoypable&);
    const NonCoypable& operator=(const NonCoypable&);
};

#endif //NONCOYPABLE_H_