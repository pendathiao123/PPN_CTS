#include<iostream>
#include<string>

class bot : Crypto
{
public:
    bot(char currency);
    ~bot();
    void trading();
    void investing();
    void sellCrypto(char, double);
    void buyCrypto(char, double);
};

bot::bot(char currency)
{
}

bot::~bot()
{
}
