#pragma once

#include <future>
#include <iostream>
#include <string>
#include <utility>

#include "HTTPRequest.hpp"
#include "IURLRequest.hpp"

#include "defs.hpp"
#include "logger.hpp"
#include "token.hpp"

void SendGetRequest(const std::string& pUrl)
{
    try
    {
        HttpURL url {pUrl};
        HTTPRequest& httpRequest = HTTPRequest::instance();
        httpRequest.get(
            url,
            [](const std::string& response) { Logger::log("HTTP REQUEST] [GET RESPONSE", response); },
            [](const std::string& error, const long code)
            { Logger::log("HTTP REQUEST] [GET RESPONSE", error + " with code " + std::to_string(code)); });
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void SendPostRequest(const std::string& pUrl, const std::string& data)
{
    try
    {
        HttpURL url {pUrl};
        HTTPRequest& httpRequest = HTTPRequest::instance();
        httpRequest.post(
            url,
            data,
            [](const std::string& response) { Logger::log("HTTP REQUEST] [POST RESPONSE", response); },
            [](const std::string& error, const long code)
            { Logger::log("HTTP REQUEST] [POST RESPONSE", error + " with code " + std::to_string(code)); });
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void SendLoginRequest(const std::string& pUrl, const std::string& uuid, const std::string& password, std::string& token)
{
    try
    {
        HttpURL url {pUrl + "/login"};
        HTTPRequest& httpRequest = HTTPRequest::instance();
        std::string data = uuidKey + uuid + "&" + passwordKey + password;
        httpRequest.post(
            url,
            data,
            [&token](const std::string& response)
            {
                Logger::log("HTTP REQUEST] [LOGIN RESPONSE", response);
                token = response;
            },
            [](const std::string& error, const long code)
            { Logger::log("HTTP REQUEST] [LOGIN RESPONSE", error + " with code " + std::to_string(code)); });
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

bool SendStatelessRequest(const std::string& pUrl,
                          const std::string& uuid,
                          const std::string& token,
                          const std::string& event)
{
    try
    {
        std::string authHeader = "Authorization: " + bearerPrefix + token;
        auto HeadersWithToken = DEFAULT_HEADERS;
        HeadersWithToken.insert(authHeader);

        HttpURL url {pUrl + "/stateless"};
        HTTPRequest& httpRequest = HTTPRequest::instance();
        std::string data = uuidKey + uuid + "&" + eventKey + event;
        bool success = false;
        httpRequest.post(
            url,
            data,
            [&success](const std::string& response)
            {
                Logger::log("HTTP REQUEST] [STATELESS RESPONSE", response);
                success = true;
            },
            [&success](const std::string& error, const long code)
            {
                Logger::log("HTTP REQUEST] [STATELESS RESPONSE", error + " with code " + std::to_string(code));
                success = false;
            },
            "",
            HeadersWithToken);
        return success;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

std::pair<bool, std::string>
SendCommandsRequest(const std::string& pUrl, const std::string& uuid, const std::string& password, std::string& token)
{
    try
    {
        std::string authHeader = "Authorization: " + bearerPrefix + token;
        auto HeadersWithToken = DEFAULT_HEADERS;
        HeadersWithToken.insert(authHeader);

        HttpURL url {pUrl + "/commands" + "?" + uuidKey + uuid};
        HTTPRequest& httpRequest = HTTPRequest::instance();

        std::promise<std::pair<bool, std::string>> promise;
        auto future = promise.get_future();

        httpRequest.get(
            url,
            [&promise](const std::string& response)
            {
                Logger::log("HTTP REQUEST] [COMMAND RECEIVED", response);
                promise.set_value({true, response});
            },
            [&promise, pUrl, uuid, password, &token](const std::string& error, const long code)
            {
                Logger::log("HTTP REQUEST] [COMMAND REQUEST FAILED", error + " with code " + std::to_string(code));
                if (code == 401)
                {
                    SendLoginRequest(pUrl, uuid, password, token);
                }
                promise.set_value({false, error});
            },
            "",
            HeadersWithToken);
        return future.get();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return {false, e.what()};
    }
}

std::pair<bool, std::string> SendRegisterRequest(const std::string& pUrl,
                                                 const std::string& uuid,
                                                 const std::string& name,
                                                 const std::string& ip,
                                                 std::string& token)
{
    try
    {
        std::string authHeader = "Authorization: " + bearerPrefix + token;
        auto HeadersWithToken = DEFAULT_HEADERS;
        HeadersWithToken.insert(authHeader);

        HttpURL url {pUrl + "/agents"};
        HTTPRequest& httpRequest = HTTPRequest::instance();
        std::string data = uuidKey + uuid + "&" + nameKey + name + "&" + ipKey + ip;
        std::promise<std::pair<bool, std::string>> promise;
        auto future = promise.get_future();

        httpRequest.post(
            url,
            data,
            [&promise](const std::string& response)
            {
                Logger::log("HTTP REQUEST] [REGISTER PROCESSED", response);
                promise.set_value({true, response});
            },
            [&promise](const std::string& error, const long code)
            {
                Logger::log("HTTP REQUEST] [REGISTER REQUEST FAILED", error + " with code " + std::to_string(code));
                promise.set_value({false, error});
            },
            "",
            HeadersWithToken);
        return future.get();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return {false, e.what()};
    }
}
