#include <my_lib.h>

int main(){

    char* write_data = "2 "; //example arduino write
    char* change_size = "a ";  //example arduino set size

    set_size(change_size);
    press_keys(write_data);

    return 0;
}