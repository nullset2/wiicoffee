#ifndef HTTPCALLS_H
#define HTTPCALLS_H

#include <vector>
#include <string>
#include <map>

struct Variant {
    std::string id;
    std::string name;
    int price;
};

struct Product {
    std::string id;
    std::string name;
    std::string description;
    int order;
    std::string subscription;
    std::vector<Variant> variants;
    std::map<std::string, std::string> tags;
};

char* http_get(char* url);
std::vector<Product> parseProductsJson(const char* json);

#endif