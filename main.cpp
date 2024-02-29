#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <charconv>
#include <regex>
#include "print.h"


struct JSONObject;




using JSONDict = std::unordered_map<std::string, JSONObject>;
using JSONList = std::vector<JSONObject>;

struct JSONObject{
    std::variant<                // variant 多类型值 类型安全的联合体
        std::nullptr_t,
        bool,
        int,
        double,
        std::string,             //  "h"
        JSONList,                //  []
        JSONDict                 //  {"hello":985} 自己引用自己得放到堆上，实际上存储的是指针
        >inner;

    void do_print() const{
        print(inner);
    }

    template <class T>
    bool is() const {
        return std::holds_alternative<T>(inner);    
    }

    template <class T>
    T const &get() const{
        return std::get<T>(inner);
    }

    template <class T>
    T &get() {
        return std::get<T>(inner);
    }

};



template <class T>
std::optional<T> try_parse_num(std::string_view str){
    T value;
    auto res = std::from_chars(str.data(), str.data() + str.size(),value);
    if (res.ec == std::errc() && res.ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

char unescape(char ch){                                                          //字符串转义
    switch(ch){
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case 'b': return '\b';
        case 'f': return '\f';
        case '0': return '\0';
        case 'v': return '\v';
        case 'a': return '\a';
        default: return ch;
    }
}
std::pair<JSONObject, size_t> parse(std::string_view json){
    if(json.empty()){  // 判断json为空
        return {JSONObject{std::nullptr_t{}},0};
    }
    else if(size_t off = json.find_first_not_of(" \n\r\t\v\f\0"); off!=0 && off != json.npos) 
    {
        auto [obj, eaten] = parse(json.substr(off));
        return {std::move(obj), eaten + off};
    }    
    else if('0' <= json[0] && json[0] <= '9' || json[0] == '-' || json[0] == '+')  //数字，科学计数
    {
        std::regex num_re{"[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?"};
        std::cmatch match;
        if (std::regex_search(json.data(), json.data()+json.size(),match, num_re)){
            std::string str = match.str();
            if(auto num = try_parse_num<int>(str); num.has_value()){  //C++17 引入 if语句的初始化，简化num要被定义俩次的问题
                return {JSONObject{num.value()}, str.size()};
            }
            if(auto num = try_parse_num<double>(str)){ // 简化
                return {JSONObject{*num}, str.size()};
            }
        }
        
        return {JSONObject{std::nullptr_t{}},0};
    }
    else if('"' == json[0])                                                        // 字符串
    {
        std::string str;
        enum{
            Raw,
            Escaped
        }phase = Raw;
        size_t i;
        for(i = 1 ; i<json.size();i++)
        {
            char ch = json[i];
            if(phase == Raw)
            {
                if(ch == '\\'){
                    phase = Escaped;
                }else if (ch == '"')
                {
                    i += 1;                    //最后一个引号 长度加一
                    break;
                }
                else {
                    str += ch;
                }
            }
            else if (phase == Escaped)
            {
                str += unescape(ch);
                phase = Raw;
            }
        }
        return {JSONObject{std::move(str)}, i};
    }
    else if('[' == json[0])                                                       // 列表
    {
        std::vector<JSONObject> vec;
        size_t i;
        for(i=1; i<json.size();)
        {
            if(json[i]==']')
            {
                i += 1;
                break;
            }
 
            auto [obj, eaten] = parse(json.substr(i));   // 递归下去

            if(eaten == 0)
            {
                i = 0;
                break;
            }

            vec.push_back(obj);  //[xx,xx,xx]，将xx入队
            i += eaten;
            if (json[i] == ',')
            {
                i += 1;
            }

        }
        return {JSONObject{std::move(vec)}, i};
    }
    else if('{' ==  json[0])
    {
        std::unordered_map<std::string, JSONObject> map;
        size_t i;
        for(i=1; i<json.size();)
        {
            if(json[i] == '}')
            {
                i += 1;
                break;
            }
            auto [key, key_eaten] = parse(json.substr(i));
            if(key_eaten == 0)
            {
                i = 0;
                break;
            }
            i += key_eaten;                                       // 加上key的值 
            if(!std::holds_alternative<std::string>(key.inner))   // 目前只支持string
            {
                i = 0;
                break;
            }
            if(json[i] != ':')                                    // 判断是否冒号
            {
                i = 0;
                break;
            }
            i += 1;
            auto [value, value_eaten] = parse(json.substr(i));
            if(value_eaten == 0)
            {
                i = 0;
                break;
            }
            i += value_eaten;
            std::string key_str = std::move(std::get<std::string>(key.inner));
            map.try_emplace(std::move(key_str), std::move(value));

            if(json[i] == ',')                                   // 逗号是下一个键值对
            {
                i += 1;
            }
        }
        return {JSONObject{std::move(map)}, i};
    }


    return {JSONObject{std::nullptr_t{}},0};

}

struct Functor{                    //  属于测试7
    void operator() (int val) const {
        print("int is",val);
    }
    void operator() (double val) const {
        print("double is",val);
    }
    void operator() (std::string val) const {
        print("string is",val);
    }
    template<class T>                        //  必须有 
    void operator() (T val) const {
        print("other",val);
    }
};


/* 测试8
// c++11方式：模板递归+继承
template <class... Fs>
struct overloaded;

template <class F1>
struct overloaded<F1> : F1 {
    overloaded(F1 f1) : F1(f1) {}
    using F1::operator();
};

template <class F1, class... Fs>
struct overloaded<F1, Fs...> : F1, overloaded<Fs...> {
    overloaded(F1 f1, Fs... fs) : F1(f1), overloaded<Fs...>(fs...) {}
    using F1::operator();
    using overloaded<Fs...>::operator();
};

template <class... Fs>
overloaded<Fs...> make_overloaded(Fs... fs) {
    return overloaded<Fs...>(fs...);
}

*/
// 折叠表达式 + 模板
template <class ...Fs>
struct overloaded : Fs...{
    using Fs::operator()...;
};

template <class ...Fs>
overloaded(Fs...) -> overloaded<Fs...>;


int main(int argc, const char** argv) {
    /*
        测试1
    */
    std::string_view str3 = R"json({ 
        "json":  1,
        "ffnm": 1,
        "jj":  ["1",  2],
        
        })json";   //R字符串 里面可以写任意/ 
    auto [obj, eaten] = parse(str3);
    print(obj);

    /*
        测试2
    */
    print(obj.is<JSONDict>());
    print(obj.is<JSONList>());
    print(obj.is<std::string>());
    auto const &dict = obj.get<JSONDict>();
    print("test : ", dict.at("json").get<int>());
    

    /*
        测试3  
    */
    auto const& school = dict.at("jj");
    auto visitschool = [](auto const& subschool){
        print("visitschool: ", subschool);
    };           // 匿名函数 自动转发 

    if(school.is<JSONList>()){
        for(auto const& subschool : school.get<JSONList>()){
            visitschool(subschool);
        }
    }
    else if(school.is<int>())
    {
         visitschool(school);
    }
    
    std::visit(visitschool, school.inner);     //或者 visit 用于多态访问   无法展开
    
    /*
        测试4
    */

    auto visitschool_l = [](auto const& school){
       if constexpr(std::is_same_v<std::decay_t<decltype(school)>, JSONList>){
           for(auto const& subschool : school){
               print("visitschool_l: ", subschool);
           }
       }
       else if constexpr(std::is_same_v<decltype(school), int>){
           print("visitschool_l: ", school);
       }
    };   
    std::visit(visitschool_l, school.inner); 

    /*
        测试5  匿名函数递归展开 
    */
/*
    全局函数出去 可以递归展开
*/
//    void dovisit (JSONObject const& school){
//     std::visit([] (auto const &school){
//        if constexpr(std::is_same_v<std::decay_t<decltype(school)>, JSONList>){
//            for(auto const& subschool : school){
//                 dovisit(subschool);
//            }
//        }
//        else {
//               print("dovisit: ", school);
//        }
//        },school.inner);
//    };
    // dovisit(school);
    
    /*
        测试6  匿名函数递归
    */
   auto dovisit = [&]  (auto &dovisit, JSONObject const& school) -> void{      // 必须给定类型
    std::visit([&] (auto const &school){
       if constexpr(std::is_same_v<std::decay_t<decltype(school)>, JSONList>){
           for(auto const& subschool : school){
                dovisit(dovisit,subschool);
           }
       }
       else {
              print("dovisit: ", school);
       }
       },school.inner);
   };
     dovisit(dovisit,school);


     /*
        测试7
    */
   std::string_view st = R"json(999.1)json";
   auto [obj1, eaten1] = parse(st);

    std::visit(Functor(), obj1.inner);

    /*
        测试8  C++17 overload技术
    */

   std::visit( 
    overloaded{
        [&](int val){
            print("int is",val);
        },
        [&](double val){
            print("double is",val);
        },
        [&](std::string val){
            print("string is",val);
        },
        [&](auto val){
            print("unknown is",val);
        }
    },
    obj1.inner
   );

    return 0;
}

