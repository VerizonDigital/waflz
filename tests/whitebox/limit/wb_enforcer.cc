//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_enforcer.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/06/2016
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: includes
//: ----------------------------------------------------------------------------
#include "catch/catch.hpp"
#include "jspb/jspb.h"
#include "support/ndebug.h"
#include "waflz/limit/enforcer.h"
#include "waflz/rqst_ctx.h"
#include "waflz/def.h"
#include "limit.pb.h"
#include <string.h>
#include <unistd.h>
//: ----------------------------------------------------------------------------
//: Config
//: ----------------------------------------------------------------------------
#define VALID_ENFORCEMENT_CONFIG_JSON "{"\
    "\"name\": \"CAS POST TEST COORDINATOR CONF-c3b05b0a-ae93-4aa6-b804-72d31137ac3f\"," \
    "\"enabled_date\": \"02/19/2016\"," \
    "\"tuples\": ["\
        "{\"rules\": ["\
          "{\"variable\": [{\"type\": \"REMOTE_ADDR\"}],"\
           "\"operator\": {\"type\": \"STREQ\", \"is_negated\": false, \"value\": \"192.16.26.2\"}," \
           "\"chained_rule\": [" \
               "{\"variable\": [{\"type\": \"REQUEST_HEADERS\", \"match\": [{\"value\": \"User-Agent\"}]}]," \
                "\"operator\": {\"type\": \"STREQ\", \"is_negated\": false, \"value\": \"braddock version ASS.KICK.IN\"}}" \
        "]}]," \
         "\"enforcements\": [" \
             "{\"name\": \"COOL ACTION NAME\", \"url\": \"https://www.google.com\", \"duration_sec\": 140, \"percentage\": 75.0, \"type\": \"redirect-302\", \"id\": \"caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD\"}]," \
         "\"id\": \"640b3c22-4b68-4b9c-b644-ada917411769AAFD\"," \
         "\"disabled\": false, "\
         "\"start_epoch_msec\": 1455848485000}]," \
  "\"customer_id\": \"DEADDEAD\"," \
  "\"id\": \"181fdc47-d78b-4344-9c43-6cea2d92d3b5AAFD\"," \
  "\"type\": \"ddos-enforcement\"" \
"}"
//: ----------------------------------------------------------------------------
//: Config
//: ----------------------------------------------------------------------------
#define MATCH_URI_CONFIG "{"\
    "\"name\": \"CAS POST TEST COORDINATOR CONF-c3b05b0a-ae93-4aa6-b804-72d31137ac3f\"," \
    "\"enabled_date\": \"02/19/2016\"," \
    "\"tuples\": ["\
        "{\"rules\": ["\
          "{\"variable\": [{\"type\": \"REQUEST_URI\"}],"\
           "\"operator\": {\"type\": \"STREQ\", \"is_negated\": false, \"value\": \"/bananas/monkey\"}" \
        "}]," \
         "\"enforcements\": [" \
             "{\"name\": \"COOL ACTION NAME\", \"url\": \"https://www.google.com\", \"duration_sec\": 2, \"percentage\": 75.0, \"type\": \"redirect-302\", \"id\": \"caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD\"}]," \
         "\"id\": \"640b3c22-4b68-4b9c-b644-ada917411769AAFD\"," \
         "\"disabled\": false, "\
         "\"start_epoch_msec\": 1455848485000}]," \
  "\"customer_id\": \"DEADDEAD\"," \
  "\"id\": \"181fdc47-d78b-4344-9c43-6cea2d92d3b5AAFD\"," \
  "\"type\": \"ddos-enforcement\"" \
"}"
//: ----------------------------------------------------------------------------
//: Config
//: ----------------------------------------------------------------------------
#define MATCH_URI_OR_CONFIG "{"\
    "\"name\": \"CAS POST TEST COORDINATOR CONF-c3b05b0a-ae93-4aa6-b804-72d31137ac3f\"," \
    "\"enabled_date\": \"02/19/2016\"," \
    "\"tuples\": ["\
        "{\"rules\": ["\
            "{\"variable\": [{\"type\": \"REQUEST_URI\"}],"\
             "\"operator\": {\"type\": \"STREQ\", \"is_negated\": false, \"value\": \"/bananas/monkey\"}},"\
             "{\"variable\": [{\"type\": \"REQUEST_URI\"}],"\
             "\"operator\": {\"type\": \"STREQ\", \"is_negated\": false, \"value\": \"/bonkers/monkey\"}}"\
         "]," \
         "\"enforcements\": [" \
             "{\"name\": \"COOL ACTION NAME\", \"url\": \"https://www.google.com\", \"duration_sec\": 140, \"percentage\": 75.0, \"type\": \"redirect-302\", \"id\": \"caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD\"}]," \
         "\"id\": \"640b3c22-4b68-4b9c-b644-ada917411769AAFD\"," \
         "\"disabled\": false, "\
         "\"start_epoch_msec\": 1455848485000}]," \
  "\"customer_id\": \"DEADDEAD\"," \
  "\"id\": \"181fdc47-d78b-4344-9c43-6cea2d92d3b5AAFD\"," \
  "\"type\": \"ddos-enforcement\"" \
"}"
//: ----------------------------------------------------------------------------
//: Config
//: ----------------------------------------------------------------------------
#define MATCH_URI_REGEX_CONFIG "{"\
    "\"name\": \"CAS POST TEST COORDINATOR CONF-c3b05b0a-ae93-4aa6-b804-72d31137ac3f\"," \
    "\"enabled_date\": \"02/19/2016\"," \
    "\"tuples\": ["\
        "{\"rules\": ["\
          "{\"variable\": [{\"type\": \"REQUEST_URI\"}],"\
           "\"operator\": {\"type\": \"RX\", \"is_negated\": false, \"value\": \"/bananas*\", \"is_regex\": true}" \
        "}]," \
         "\"enforcements\": [" \
             "{\"name\": \"COOL ACTION NAME\", \"url\": \"https://www.google.com\", \"duration_sec\": 140, \"percentage\": 75.0, \"type\": \"redirect-302\", \"id\": \"caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD\"}]," \
         "\"id\": \"640b3c22-4b68-4b9c-b644-ada917411769AAFD\"," \
         "\"disabled\": false, "\
         "\"start_epoch_msec\": 1455848485000}]," \
  "\"customer_id\": \"DEADDEAD\"," \
  "\"id\": \"181fdc47-d78b-4344-9c43-6cea2d92d3b5AAFD\"," \
  "\"type\": \"ddos-enforcement\"" \
"}"
//: ----------------------------------------------------------------------------
//: Config
//: ----------------------------------------------------------------------------
#define MATCH_NO_RULES_CONFIG "{"\
    "\"name\": \"CAS POST TEST COORDINATOR CONF-c3b05b0a-ae93-4aa6-b804-72d31137ac3f\"," \
    "\"enabled_date\": \"02/19/2016\"," \
    "\"tuples\": ["\
         "{"\
         "\"enforcements\": [" \
             "{\"name\": \"COOL ACTION NAME\", \"url\": \"https://www.google.com\", \"duration_sec\": 140, \"percentage\": 75.0, \"type\": \"redirect-302\", \"id\": \"caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD\"}]," \
         "\"id\": \"640b3c22-4b68-4b9c-b644-ada917411769AAFD\"," \
         "\"disabled\": false, "\
         "\"start_epoch_msec\": 1455848485000}]," \
  "\"customer_id\": \"DEADDEAD\"," \
  "\"id\": \"181fdc47-d78b-4344-9c43-6cea2d92d3b5AAFD\"," \
  "\"type\": \"ddos-enforcement\"" \
"}"
//: ----------------------------------------------------------------------------
//: enforcer
//: ----------------------------------------------------------------------------
TEST_CASE( "enforcer test", "[enforcer]" ) {
        // -------------------------------------------------
        // bad config
        // -------------------------------------------------
        SECTION("verify load failures bad json 1") {
                int32_t l_s;
                const char l_json[] = "woop woop [[[ bloop {##{{{{ ]} blop blop %%# &(!(*&!#))";
                ns_waflz::enforcer l_e;
                l_s = l_e.load(l_json, sizeof(l_json));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_ERROR));
        }
        // -------------------------------------------------
        // bad config
        // -------------------------------------------------
        SECTION("verify load failures bad json 2") {
                int32_t l_s;
                const char l_json[] = "blorp";
                ns_waflz::enforcer l_e;
                l_s = l_e.load(l_json, sizeof(l_json));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_ERROR));
        }
        // -------------------------------------------------
        // bad config
        // -------------------------------------------------
        SECTION("verify load failures bad json 3") {
                int32_t l_s;
                const char l_json[] = "[\"b\", \"c\",]";
                ns_waflz::enforcer l_e;
                l_s = l_e.load(l_json, sizeof(l_json));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_ERROR));
        }
        // -------------------------------------------------
        // valid json bad config
        // -------------------------------------------------
        SECTION("verify load failures valid json -bad config") {
                int32_t l_s;
                const char l_json[] = "{\"b\": \"c\"}";
                ns_waflz::enforcer l_e;
                l_s = l_e.load(l_json, sizeof(l_json));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_ERROR));
        }
        // -------------------------------------------------
        // Valid config
        // -------------------------------------------------
        SECTION("verify load success") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(VALID_ENFORCEMENT_CONFIG_JSON, sizeof(VALID_ENFORCEMENT_CONFIG_JSON));
                //NDBG_PRINT("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
        }
        // -------------------------------------------------
        // Simple Match URI
        // -------------------------------------------------
        SECTION("verify simple URI match") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(MATCH_URI_CONFIG, sizeof(MATCH_URI_CONFIG));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                const waflz_limit_pb::enforcement* l_enfx = NULL;
                ns_waflz::rqst_ctx l_ctx(NULL, 0);
                // Verify match
                l_ctx.m_uri.m_data = "/bananas/monkey";
                l_ctx.m_uri.m_len = strlen("/bananas/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                // Verify no match
                l_ctx.m_uri.m_data = "/bonkers/monkey";
                l_ctx.m_uri.m_len = sizeof("/bonkers/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx == NULL));
        }
        // -------------------------------------------------
        // Match AND config
        // -------------------------------------------------
        SECTION("verify match AND config") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(VALID_ENFORCEMENT_CONFIG_JSON, sizeof(VALID_ENFORCEMENT_CONFIG_JSON));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                const waflz_limit_pb::enforcement* l_enfx = NULL;
                ns_waflz::rqst_ctx l_ctx(NULL, 0);
                // -----------------------------------------
                // add user-agent
                // -----------------------------------------
                ns_waflz::data_t l_d_k;
                l_d_k.m_data = "User-Agent";
                l_d_k.m_len = strlen("User-Agent");
                ns_waflz::data_t l_d_v;
                l_d_v.m_data = "braddock version ASS.KICK.IN";
                l_d_v.m_len = strlen("braddock version ASS.KICK.IN");
                l_ctx.m_header_map[l_d_k] = l_d_v;
                // -----------------------------------------
                // add ip
                // -----------------------------------------
                l_ctx.m_src_addr.m_data = "192.16.26.2";
                l_ctx.m_src_addr.m_len = strlen("192.16.26.2");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                // -----------------------------------------
                // wrong ip
                // -----------------------------------------
                l_ctx.m_src_addr.m_data = "192.16.26.3";
                l_ctx.m_src_addr.m_len = strlen("192.16.26.3");
                l_enfx = NULL;
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
        }
        // -------------------------------------------------
        // Match OR config
        // -------------------------------------------------
        SECTION("verify match OR config") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(MATCH_URI_OR_CONFIG, sizeof(MATCH_URI_OR_CONFIG));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                const waflz_limit_pb::enforcement* l_enfx = NULL;
                ns_waflz::rqst_ctx l_ctx(NULL, 0);
                l_ctx.m_uri.m_data = "/bananas/monkey";
                l_ctx.m_uri.m_len = strlen("/bananas/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                // Verify match
                l_ctx.m_uri.m_data = "/bonkers/monkey";
                l_ctx.m_uri.m_len = strlen("/bonkers/monkey");
                l_enfx = NULL;
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
        }
        // -------------------------------------------------
        // Match Regex config
        // -------------------------------------------------
        SECTION("verify match regex config") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(MATCH_URI_REGEX_CONFIG, sizeof(MATCH_URI_REGEX_CONFIG));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                const waflz_limit_pb::enforcement* l_enfx = NULL;
                ns_waflz::rqst_ctx l_ctx(NULL, 0);
                // verify match
                l_ctx.m_uri.m_data = "/bananas/monkey";
                l_ctx.m_uri.m_len = strlen("/bananas/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                // Verify no match
                l_ctx.m_uri.m_data = "/bonkers/monkey";
                l_ctx.m_uri.m_len = sizeof("/bonkers/monkey");
                l_enfx = NULL;
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx == NULL));
        }
        // -------------------------------------------------
        // No rules
        // -------------------------------------------------
        SECTION("verify match no rules config") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(MATCH_NO_RULES_CONFIG, sizeof(MATCH_NO_RULES_CONFIG));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                const waflz_limit_pb::enforcement* l_enfx = NULL;
                ns_waflz::rqst_ctx l_ctx(NULL, 0);
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
        }
        // -------------------------------------------------
        // verify expiration
        // -------------------------------------------------
        SECTION("verify expiration") {
                int32_t l_s;
                ns_waflz::enforcer l_e;
                l_s = l_e.load(MATCH_URI_CONFIG, sizeof(MATCH_URI_CONFIG));
                //printf("err: %s\n", l_e.get_err_msg());
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                const waflz_limit_pb::enforcement* l_enfx = NULL;
                ns_waflz::rqst_ctx l_ctx(NULL, 0);
                l_e.update_start_time();
                // verify match
                l_ctx.m_uri.m_data = "/bananas/monkey";
                l_ctx.m_uri.m_len = strlen("/bananas/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                // verify no match
                l_ctx.m_uri.m_data = "/bonkers/monkey";
                l_ctx.m_uri.m_len = sizeof("/bonkers/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx == NULL));
                // verify match
                l_ctx.m_uri.m_data = "/bananas/monkey";
                l_ctx.m_uri.m_len = strlen("/bananas/monkey");
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx != NULL));
                REQUIRE((l_enfx->id() == "caa9be38-35cf-465c-bf61-7e99f2eea30bAAFD"));
                // sleep 3 -wait for expiration
                sleep(3);
                l_s = ns_waflz::limit_sweep(*(l_e.get_mutable_pb()));
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                l_s = l_e.process(&l_enfx, &l_ctx);
                REQUIRE((l_s == WAFLZ_STATUS_OK));
                REQUIRE((l_enfx == NULL));
        }
        // TODO negated
}
