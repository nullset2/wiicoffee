#include "httpcalls.h"
#include <wiisocket.h>
#include <stdio.h>
#include <curl/curl.h>
#include <mbedtls/sha1.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gctypes.h>
#include <gccore.h>

#include <json/json.h>
#include <string>
#include <vector>
#include <map>

#include "cacert_pem.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static struct curl_blob ca_info = {
	.data = (u8*)cacert_pem,
	.len = cacert_pem_size,
	.flags = CURL_BLOB_COPY,
};

struct memory {
  char *response;
  size_t size;
};

static size_t cb(void *data, size_t size, size_t nmemb, void *clientp) {
	size_t realsize = size * nmemb;
	struct memory *mem = (struct memory *)clientp;

	char *ptr = realloc(mem->response, mem->size + realsize + 1);
	if(ptr == NULL){
		puts("OOM");
		return 0;
	}

	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;

	return realsize;
}

std::vector<Product> parseProductsJson(const char* jsonString) {
    std::vector<Product> products;
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    
    bool parsingSuccessful = reader->parse(
        jsonString,
        jsonString + strlen(jsonString),
        &root,
        &errors
    );
    delete reader;
    
    if (!parsingSuccessful) {
        puts( "Error parsing JSON. ");
        return products;
    }
    
    const Json::Value& data = root["data"];
    if (!data.isArray()) {
        puts("Expected 'data' to be an array");
        return products;
    }
    
    for (const auto& productJson : data) {
        Product product;
        
        product.id = productJson["id"].asString();
        product.name = productJson["name"].asString();
        product.description = productJson["description"].asString();
        product.order = productJson["order"].asInt();
        
        if (productJson.isMember("subscription")) {
            product.subscription = productJson["subscription"].asString();
        }
        
        const Json::Value& variants = productJson["variants"];
        for (const auto& variantJson : variants) {
            Variant variant;
            variant.id = variantJson["id"].asString();
            variant.name = variantJson["name"].asString();
            variant.price = variantJson["price"].asInt();
            product.variants.push_back(variant);
        }
        
        const Json::Value& tags = productJson["tags"];
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            const std::string& tagName = it.key().asString();
            const std::string& tagValue = (*it).asString();
            product.tags[tagName] = tagValue;
        }
        
        products.push_back(product);
    }
    
    return products;
}

char* http_get(char* path) {
    const char* jsonData = R"({
        "data": [
            {
                "id": "prd_01JD0E7PD4H3XDZA5P5VXSDPQC",
                "name": "cron",
                "description": "Subscribe to cron, our monthly coffee subscription.",
                "order": 1,
                "subscription": "required",
                "variants": [
                    {
                        "id": "var_01JD0E87SB7K9MB5KGFPVJ1N7A",
                        "name": "12oz",
                        "price": 3000
                    }
                ],
                "tags": {
                    "featured": true,
                    "market_eu": false,
                    "market_na": true
                }
            }
        ]
	})";
	return jsonData;
}

/*
char* http_get(char* path) {
	int socket_init_success = -1;
	for (int attempts = 0; attempts < 3; attempts++) {
		socket_init_success = wiisocket_init();
		printf("attempt: %d wiisocket_init: %d\n", attempts, socket_init_success);
		if (socket_init_success == 0)
			break;
	}
	if (socket_init_success != 0) {
		puts("failed to init wiisocket");
		return NULL;
	}

	u32 ip = 0;
	for (int attempts = 0; attempts < 3; attempts++) {
		ip = gethostid();
		printf("attempt: %d gethostid: %x\n", attempts, ip);
		if (ip)
			break;
	}
	if (!ip) {
		puts("failed to get ip");
		return NULL;
	}
	CURL* curl = curl_easy_init();
	if (!curl){
		puts("curl easy init failed");
		return NULL;
	}

	char url[256];  // Adjust size as needed
	snprintf(url, sizeof(url), "https://api.dev.terminal.shop%s", path);

	struct memory chunk = {0};
	chunk.response = malloc(1);
	chunk.size = 0;

	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Authorization: Bearer ...");

	char err[CURL_ERROR_SIZE + 1] = {0};

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "HTTP,HTTPS");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
	curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca_info);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	printf("Response:\n%s\n", chunk.response);

    if (res == CURLE_OK) {
		Json::Value json = parseProductsJson(chunk.response);
		return json;
	}
	else {
		free(chunk.response);
		return NULL;
	}
}*/

