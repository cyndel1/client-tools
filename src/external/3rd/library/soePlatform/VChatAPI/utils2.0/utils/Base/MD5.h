// MD5.h: interface for the MD5 class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MD5_H
#define MD5_H

#include <string>
#include <vector>


namespace soe 
{


    class MD5  
    {
        struct State 
        {
            State();

            std::vector<int> state;
            std::vector<int> count;
            std::vector<char> buffer;
        };

        public:
            MD5::MD5();
	        MD5(const std::string &input);

            void Init();
            std::vector<char> Final();
            void Update(char char0);
            void Update(State & state1, std::vector<char> achar0, int i, int j);
            void Update(std::string s);
            void Update(std::vector<char> achar0);
            void Update(std::vector<char> achar0, int i);
            void Update(std::vector<char> achar0, int i, int j);
            std::string asHex();
            static std::string asHex(std::vector<char> achar0);

        private:
            std::vector<int> Decode(std::vector<char> achar0, int i, int j);
            std::vector<char> Encode(std::vector<int> ai, int i);
            int FF(int i, int j, int k, int l, int i1, int j1, int k1);
            int GG(int i, int j, int k, int l, int i1, int j1, int k1);
            int HH(int i, int j, int k, int l, int i1, int j1, int k1);
            int II(int i, int j, int k, int l, int i1, int j1, int k1);
            void Transform(State & state1, std::vector<char> achar0, int i);
            int rotate_left(int i, int j);
            int uadd(int i, int j);
            int uadd(int i, int j, int k);
            int uadd(int i, int j, int k, int l);

        private:
            State   state;
            State   finals;
            bool    finalsNull;
            static std::vector<char> padding;

    };


};

#endif  //  MD5_H
