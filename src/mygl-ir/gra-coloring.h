#include<vector>
#include<stack>
#include<set>
using namespace std;

class variety{
    public:
        int id;
        int node_color;
        int weight;   //储存每一个结点的溢出权重
        
        variety(int n){
            id = n;
            node_color = -1;
            weight = 0;
        }
};

class gragh{
    private:
        vector<variety> register_allocated;
        vector<set<int>> M_edge;
        stack<vector<int>> stk; //stack用来装临时被清除的节点，vector中的第一位表示节点序号
    public:
        vector<set<int>> adjList;
        int node_num=adjList.size();
        
        gragh(int n){
            node_num=n;
            adjList.resize(n);
            for(int i=0;i<n;i++){
                class variety v(i);
                register_allocated.push_back(v);
            }
        }

        void addEdge(int u,int v);//为图中加入新的一条边

        void addColors(int node,int color); //为一个节点加入其相邻颜色

        int choose(int node,int k);//给定一个结点和相邻颜色列表，为结点选择一个颜色；

        bool find_edge(int u,int v);  //验证图中是否有这条边

        int degree(int n);  //返回一个结点的度数

        void spill(int index);

        void print();

        vector<int> pop_stk();
        
        set<int> get_neibor(int n){
            set<int> temp(adjList[n]);
            return temp;
        };

        void delete_node(int n);//在图中删除一个结点

        vector<set<int>> get_medge(){
            auto temp= M_edge;
            return temp;
        }
        void merge(int b,int c);
};  

class RegAllocationContext{
    public:
        vector<vector<int>> live;  //储存每个语句中活跃的节点

    public:
        int color_num ;
        
        RegAllocationContext(int num,vector<vector<int>> list){
            color_num = num;
            live = list;
        }  
        friend void RegAlloc(RegAllocationContext& ra,gragh& G);
        friend void build(RegAllocationContext& ra,gragh& G);
        friend void simplify(RegAllocationContext& ra,gragh& G);
        friend void coalesce(RegAllocationContext& ra,gragh& G);
        friend int select(RegAllocationContext& ra,gragh& G);
        friend void spill(RegAllocationContext& ra,gragh& G,int node);
        friend void rewrite(RegAllocationContext& ra,gragh& G);
        
};

