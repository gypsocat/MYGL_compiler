/*
图着色算法的实现。图着色算法：build -> simplify -> coalase ->select -> spill -> rewrite.
目前实现了基本的算法流程，但是在spill部分还缺少贪婪的溢出策略算法。
默认输出是一个体现各流程活跃变量的数组 如 {{1,2},{2,3},{3}}
分配后的颜色会写入 variety 类的对象中。
刘泽栋 2023/12/10

*/



#include<iostream>
#include<map>
#include<set>
#include<string.h>
#include<vector>
#include<algorithm>
#include"gra-coloring.h"

#define nodeNum 5
using namespace std;

void gragh::addEdge(int u,int v){
            adjList[u].insert(v);
            adjList[v].insert(u);
        }


int gragh::choose(int node,int k){
    vector<int> neibor_colors;
    if(varieties[node].node_color==-2) return -2;
    for(auto u:adjList[node]){
        if(varieties[u].node_color!=-1) neibor_colors.push_back(varieties[u].node_color);
    }
    for(int i=0;i<k;i++){
        for(auto u:neibor_colors){
            if(u==i) goto out;
        }
        varieties[node].node_color = i;
        return i;
        out:
            continue;
    }
    return -1;
}

bool gragh::find_edge(int u,int v){
    auto edges = adjList[u];
    if(u==v) return true;
    for(auto a:adjList[u]){
        if(a == v) return true;
    }
    return false;
}

int gragh::degree(int n){
    return adjList[n].size();
}

void gragh::delete_node(int n){
    vector<int> temp;
    temp.push_back(n);      //temp的第一个位置是变量的序号
    for(auto u:adjList[n]){
        temp.push_back(u);
    }
    adjList[n].clear();  //删除该点的所有临接边表示该节点被删除。
    stk.push(temp);
    for(auto &node:adjList){
        auto it = find(node.begin(),node.end(),n);
        if(it!=node.end()){
            node.erase(it);
        }
    }
}


void build(RegAllocation& ra,gragh& G){
    
    for(auto state : ra.live){
        int i = state[0];
        for(auto variaty:state){
            G.addEdge(i,variaty);
        }

    }
}

void simplify(RegAllocation &ra,gragh& G){
    for(int i=0;i<G.node_num;i++){   
        G.delete_node(i);
        
    } 
}

void gragh::merge(int b,int c){
            adjList[b].insert(adjList[c].begin(),adjList[c].end()); 
            adjList[c].clear();       //清空c的边代表c已经被移出图中
            for(auto &node:adjList){
                auto it = find(node.begin(),node.end(),c);
                if(it!=node.end()){
                    node.erase(it);
                    node.insert(b);
                }
            }      //在每个结点的邻接点中删除c，留下b
            
};



/*brief:判断化简之后图中是否有移动边，for每一对移动边（a，b），
      判断：a的每一个临界点c。1，c与b干涉。2，degree(c)<k。就进行接合。
      接合后得重回simplyfy.
*/
void coalesce(RegAllocation& ra,gragh& G){
    auto M = G.get_medge();
    for(auto m_edge:M){
        int a = *(m_edge.begin());
        int b = *(m_edge.rbegin());
        if(G.degree(a)!=0 && G.degree(b)!=0){
            auto neibor_a = G.get_neibor(a);
            for(auto c:neibor_a){
                if(!G.find_edge(c,b)&&G.degree(c)>ra.color_num)
                    break;
                else{
                    G.merge(b,c);
                }
            }
        }
    }
}

vector<int> gragh::pop_stk(){
            vector<int> temp;
            if(!stk.empty()){ 
                temp = stk.top();
                stk.pop();
            }

            return temp;
        }

int select(RegAllocation& ra,gragh& G){
    vector<int> node;
    while(!(node=G.pop_stk()).empty()){
        int index = node[0];
        int color = -1;
        for(auto a=node.begin()+1;a!=node.end();a++){
           G.addEdge(index,*a);
        }
        color = G.choose(index,ra.color_num);
        if(color!=-1){    
            continue;
        }
        else {
            spill(ra,G,index);
            return 0;
        }    
    }
    cout<<"register allocation finished!"<<endl;
    return 1;

}

void gragh::spill(int index){
    varieties[index].node_color = -2;  //暂时用-2表示溢出的情况
    adjList[index].clear();
}

void spill(RegAllocation& ra,gragh& G,int node){
    G.spill(node);
                                                         
}

void rewrite(RegAllocation& ra,gragh& G){
    vector<int> node;
    while(!(node=G.pop_stk()).empty()){
        int index = node[0];
        int color = -1;
        for(auto a=node.begin()+1;a!=node.end();a++){
           G.addEdge(index,*a);
        }
    }
}

void gragh::print(){
    for(auto variety : varieties){
        cout<<variety.id<<" -> %"<<variety.node_color<<endl;
    }
}
void RegAlloc(RegAllocation& ra,gragh& G){
    int flag = 0;
    build(ra,G);
    while(flag == 0){
        simplify(ra,G);
        coalesce(ra,G);
        flag = select(ra,G);
        if(flag == 0)
            rewrite(ra,G);
    }    
    G.print();
}

int main(){
    
    vector<vector<int>> list ={{0,1},{0,4},{0,3},{1,2},{1,3},{1,4},{2,3},{2,4},{3,4}};

    RegAllocation ra(3,list);
    gragh G(nodeNum);
    RegAlloc(ra,G);
    return 0;
}




