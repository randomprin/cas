#include <iostream>
#include "Tes_LockFree_Obj_Fifo.h"
#include <thread>
//#include <chrono>
class Tes_Str: public Tes_LF_Node
{
public:
    char str[40];
};
Tes_LockFree_Obj_Fifo<Tes_Str> safels;
int main() {
    //std::cout << "Hello, World!" << std::endl;
    safels.Ini_Size(50+1);//important
    std::thread p[4];
    for(int i=0;i<3;i++)
    {
        p[i] = std::thread([](){
            int do_time = 0;
            while(do_time<20){
                Tes_Memory_Outcome tmo;
                tmo.m_size = 3;
                if(safels.produce(tmo))//important
                {
                    int dolen = 3;
                    Tes_Str* tln = (Tes_Str*)tmo.m_ptr1;
                    int tid = 0;
                    while (dolen > 0)
                    {
                        sprintf(tln->str,"thread[%d] do %d",std::this_thread::get_id(),(do_time)*3 + tid);
                        tln=(Tes_Str*)tln->next;
                        tid++;
                        dolen--;
                    }
                    do_time++;
                    safels.set_produce_end(tmo);//important
                }
            }
        });
    }
    p[3] = std::thread([](){
        while(true)
        {
            Tes_Memory_Outcome tmo;
            if(safels.consume(tmo))//important
            {
                int dolen = tmo.m_size1;
                Tes_Str* tln = (Tes_Str*)tmo.m_ptr1;
                while(dolen>0)
                {
                    std::cout<<tln->str<<std::endl;
                    tln=(Tes_Str*)tln->next;
                    dolen--;
                }
                safels.set_consume_end(tmo);//important
            }
            std::this_thread::sleep_for (std::chrono::milliseconds(1));
        }
    });
    for(int i=0;i<3;i++)
    {
        p[i].join();
    }
    std::cout<<"produce end!"<<std::endl;
    getchar();
    return 0;
}