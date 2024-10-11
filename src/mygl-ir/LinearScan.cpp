#include<iostream>
#include<stdlib.h>
#include<vector>
#include<map>
#include<string.h>

using namespace std;

vector<vector<string>> variety_list = {
        {"a2", "0", "5"},
        {"b2", "1", "5"},
        {"d", "1", "8"}
    };

class RegisterAllocater{

private:
    int Reg_num;
        int interval;
    class Register{
        public:
            int id;
            bool used;
        public:
            Register(int i){
                id=i;
                used=false;
            }
            bool operator<(const Register &other) const{
                return id<other.id;
            }
    };

    class variety{
        public:
            float weight;
            vector<int> start;
            vector<int> end;
            bool isallocated;
        public:
            string name;
            variety(){
                name=" ";
                isallocated = false;
                weight = 0;
            }
            variety(string n,string s,string e){
                name = n;
                start.push_back(stoi(s));
                end.push_back(stoi(e));
                isallocated = false;
                weight = 0;
            }
            void allocate(){
                isallocated=true;
            }
            void disallocate(){
                isallocated=false;
            }
            void next(){
                start.erase(start.begin());
                end.erase(end.begin());
            }
            bool isactive(int i){
                if(!start.empty()&&start[0]==i){
                    return true;
                }
                else return false;
            }
            bool isold(int i){
                if(!end.empty()&&end[0] < i){
                    return true;
                }
                else return false;
            }
            bool isnew(int i){
                if(!start.empty()&&start[0] >= i){
                    return true;
                }
                else return false;
            }

    };
    map<Register,variety> active;
    vector<Register> free;
    vector<variety> pool;
public:
    RegisterAllocater(int reg_num,vector<vector<string>> list){
        Reg_num = reg_num; //初始化寄存器信息
        for(int i = 0;i<Reg_num;i++){
            Register R0(i);
            free.push_back(R0);
        } 
        for(int i = 0;i<list.size();i++){
            variety v(list[i][0],list[i][1],list[i][2]);
            pool.push_back(v);
        }

    }
    ~RegisterAllocater(){

    }
    bool status(){
        for(auto i=pool.begin();i!=pool.end();i++){
            if(i->isnew(interval)) return true;
        }
        return false;
    }
    bool is_work(){
        for(auto i=pool.begin();i!=pool.end();i++){
            if(i->isactive(interval)&&i->isallocated==false) return true; //判断是否还有变量需要分配
        }
        return false;
    }

    friend variety* choose(RegisterAllocater& ra,vector<variety>& pool);
    friend void assign(RegisterAllocater& ra);
    friend void remove(RegisterAllocater& ra);
    friend void spill(RegisterAllocater& ra);
    friend void print(RegisterAllocater& ra);

};

void print(RegisterAllocater& ra){
    cout<<ra.interval<<endl;
    for(auto it = ra.active.begin();it!=ra.active.end();it++){
        cout<<it->first.id<<":"<<it->second.name<<" "<<endl;
    }
    cout<<endl;
    
}

RegisterAllocater::variety* choose(RegisterAllocater& ra,vector<RegisterAllocater::variety>& pool){
    RegisterAllocater::variety* min;
    for(vector<RegisterAllocater::variety>::iterator i=pool.begin();i!=pool.end();i++){
        if(i->isactive(ra.interval)&&i->isallocated==false){
            min = &(*i);
            break;
        }
    }
    for(auto i=pool.begin();i!=pool.end();i++){
        if(i->isactive(ra.interval)&&i->end[0]<(*min).end[0]&&i->isallocated==false){
            min = &(*i);
        }
    }
    return min;
}

//为变量分配寄存器
void assign(RegisterAllocater& ra){
    while(ra.status()){
        remove(ra);
        while(ra.is_work()){
            if(ra.active.size()==ra.Reg_num){
                spill(ra);
            }
            else{
                RegisterAllocater::variety* v;
                v = choose(ra,ra.pool);
                ra.active[ra.free[0]] = *v;
                (*v).isallocated = true;
                ra.free.erase(ra.free.begin());
            }
        }
        print(ra);
        ra.interval++;
    }
}

//将旧变量剔除

void remove(RegisterAllocater& ra) {
    vector<decltype(ra.active.begin())> toRemove;

    for (auto it = ra.active.begin(); it != ra.active.end(); it++) {
        if (it->second.isold(ra.interval)) {
            ra.free.push_back(it->first);   // free该寄存器
            it->second.start.erase(it->second.start.begin());
            it->second.end.erase(it->second.end.begin());
            toRemove.push_back(it);         // 记录要删除的迭代器
        }
    }

    for (auto it : toRemove) {
        ra.active.erase(it);               // 从active中删除寄存器
    }
}

//将溢出变量
void spill(RegisterAllocater& ra){
    vector<decltype(ra.pool.begin())> toRemove;
    for(auto i  =ra.pool.begin();i!=ra.pool.end();i++){
        if(i->isactive(ra.interval)&&i->isallocated==false){             //如果有变量在当前时段需要使用，就将其溢出。
            toRemove.push_back(i);
        }
    }
    for (auto &it : toRemove) {
        ra.pool.erase(it);               // 从active中删除寄存器
    }
}



int main(){
    RegisterAllocater ra(2,variety_list);
    assign(ra);
    return 0;

}