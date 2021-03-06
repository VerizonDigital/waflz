//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! Includes
//! ----------------------------------------------------------------------------
#include "event.pb.h"
#include "profile.pb.h"
#include "action.pb.h"
#include "jspb/jspb.h"
#include "support/ndebug.h"
#include "waflz/def.h"
#include "waflz/instance.h"
#include "waflz/waf.h"
#include "waflz/rqst_ctx.h"
#include <dirent.h>
#include <string.h>
#include <errno.h>
//! ----------------------------------------------------------------------------
//! constants
//! ----------------------------------------------------------------------------
#define CONFIG_SECURITY_WAF_INSTANCE_MAX_SIZE (1024*1024)
#define CONFIG_SECURITY_WAF_PROFILE_MAX_SIZE (1024*1024)
//! ----------------------------------------------------------------------------
//! macros
//! ----------------------------------------------------------------------------
#define VERIFY_HAS(_pb, _field) do { \
        if(!_pb.has_##_field()) { \
                WAFLZ_PERROR(m_err_msg, "missing %s field", #_field); \
                return WAFLZ_STATUS_ERROR; \
        } \
} while(0)
namespace ns_waflz {
//! ----------------------------------------------------------------------------
//! utils
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details Validate custom enforcement for waf and ddos configs
//! \return  0 if it is valid
//!          false if not
//! \param   ao_err_msg  The buffer to populate with an error mesage.
//! \param   a_field TODO
//! \param   a_doc The json object whose field to validate
//! ----------------------------------------------------------------------------
static int32_t waf_config_check_enf_array(char *ao_err_msg,
                                          waflz_pb::enforcement_type_t &ao_type,
                                          const google::protobuf::RepeatedPtrField<waflz_pb::enforcement>&a_obj,
                                          const char *a_field)
{
        for(int32_t i_e = 0; i_e < a_obj.size(); ++i_e)
        {
                const waflz_pb::enforcement &l_e = a_obj.Get(i_e);
                if(!l_e.has_type())
                {
                        WAFLZ_PERROR(ao_err_msg, "error -invalid enforcement for: %s (missing type field)", a_field);
                        return WAFLZ_STATUS_ERROR;
                }
                if(l_e.type() == "nop")
                {
                        ao_type = waflz_pb::enforcement_type_t_NOP;
                }
                else if(l_e.type() == "alert")
                {
                        ao_type = waflz_pb::enforcement_type_t_ALERT;
                }
                else if(l_e.type() == "block-request")
                {
                        ao_type = waflz_pb::enforcement_type_t_BLOCK_REQUEST;
                }
                else if(l_e.type() == "custom-response")
                {
                        ao_type = waflz_pb::enforcement_type_t_CUSTOM_RESPONSE;
                }
                else if(l_e.type() == "redirect-302")
                {
                        ao_type = waflz_pb::enforcement_type_t_REDIRECT_302;
                }
                else
                {
                        return WAFLZ_STATUS_ERROR;
                }
        }
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details ctor
//! \return  None
//! \param   a_unparsed_json  The
//! ----------------------------------------------------------------------------
instance::instance(engine &a_engine):
        m_init(false),
        m_pb(NULL),
        m_err_msg(),
        m_engine(a_engine),
        m_id(),
        m_name(),
        m_customer_id(),
        m_profile_audit(NULL),
        m_profile_prod(NULL)
{
        m_pb = new waflz_pb::instance();
}
//! ----------------------------------------------------------------------------
//! \brief   dtor
//! \deatils
//! \return  None
//! ----------------------------------------------------------------------------
instance::~instance()
{
        if(m_profile_audit)
        {
                delete m_profile_audit;
                m_profile_audit = NULL;
        }
        if(m_profile_prod)
        {
                delete m_profile_prod;
                m_profile_prod = NULL;
        }
        if(m_pb)
        {
                delete m_pb;
                m_pb = NULL;
        }
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
int32_t instance::load(const char *a_buf,
                              uint32_t a_buf_len)
{
        if(a_buf_len > CONFIG_SECURITY_WAF_INSTANCE_MAX_SIZE)
        {
                WAFLZ_PERROR(m_err_msg, "config file size(%u) > max size(%u)",
                             a_buf_len,
                             CONFIG_SECURITY_WAF_INSTANCE_MAX_SIZE);
                return WAFLZ_STATUS_ERROR;
        }
        m_init = false;
        int32_t l_s;
        l_s = update_from_json(*m_pb, a_buf, a_buf_len);
        //TRC_DEBUG("whole config %s", m_pb->DebugString().c_str());
        if(l_s != JSPB_OK)
        {
                WAFLZ_PERROR(m_err_msg, "parsing json. Reason: %s", get_jspb_err_msg());
                return WAFLZ_STATUS_ERROR;
        }
        l_s = validate();
        if(l_s != WAFLZ_STATUS_OK)
        {
                return WAFLZ_STATUS_ERROR;
        }
        m_init = true;
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
int32_t instance::load(void *a_js)
{
        m_init = false;
        const rapidjson::Document &l_js = *((rapidjson::Document *)a_js);
        int32_t l_s;
        l_s = update_from_json(*m_pb, l_js);
        if(l_s != JSPB_OK)
        {
                WAFLZ_PERROR(m_err_msg, "parsing json. Reason: %s", get_jspb_err_msg());
                return WAFLZ_STATUS_ERROR;
        }
        l_s = validate();
        if(l_s != WAFLZ_STATUS_OK)
        {
                return WAFLZ_STATUS_ERROR;
        }
        m_init = true;
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \brief   Validate provided input json is valid for generating modsecurity configuration
//! \details For now we do this simply by checking that it has all
//!          the required elements.  We could use a json-schema
//!          implementation in C (http://json-schema.org/implementations.html)
//!          but we have such a nice lightweight json component, it seems
//!          a shame to bring in a huge thing just for this use
//! \return  0 if it is valid, ao_err_msg reset
//!          -1 if it is not and ao_err_msg is populated
//! \param   ao_err_msg   The string containing the error message on failure
//!                             Message will be appended -so assumes string is already empty.
//!                             ASSUMPTION: is not null and valid
//! ----------------------------------------------------------------------------
int32_t instance::validate(void)
{
        if(m_init)
        {
                return WAFLZ_STATUS_OK;
        }
        if(!m_pb)
        {
                WAFLZ_PERROR(m_err_msg, "pb == NULL");
                return WAFLZ_STATUS_ERROR;
        }
        std::string l_val;
        waflz_pb::enforcement_type_t l_action;
        const waflz_pb::instance &l_pb = *m_pb;
        // -----------------------------------------------------------
        //                     I N S T A N C E
        // -----------------------------------------------------------
        VERIFY_HAS(l_pb, id);
        VERIFY_HAS(l_pb, name);
        VERIFY_HAS(l_pb, customer_id);
        VERIFY_HAS(l_pb, enabled_date);
        // set...
        m_id = m_pb->id();
        m_name = m_pb->name();
        m_customer_id = m_pb->customer_id();
        // -----------------------------------------------------------
        //                A U D I T   P R O F I L E
        // -----------------------------------------------------------
        VERIFY_HAS(l_pb, audit_profile_action);
        l_val = l_pb.audit_profile_action();
        if(l_val == "alert")
        {
                l_action = waflz_pb::enforcement_type_t_ALERT;
        }
        else if(l_val == "block")
        {
                l_action = waflz_pb::enforcement_type_t_BLOCK_REQUEST;
        }
        else
        {
                //ERROR("Invalid audit_profile_action value: %.*s", SUBBUF_FORMAT((*m_json)["audit_profile_action"].str()));
                WAFLZ_PERROR(m_err_msg, "Invalid audit_profile_action value: %s", l_val.c_str());
                return WAFLZ_STATUS_ERROR;
        }
        if(l_pb.audit_profile_enforcements_size())
        {
                int32_t l_s;
                l_s = waf_config_check_enf_array(m_err_msg,
                                                 l_action,
                                                 l_pb.audit_profile_enforcements(),
                                                 "audit_profile_enforcements");
                if(l_s != WAFLZ_STATUS_OK)
                {
                        return WAFLZ_STATUS_ERROR;
                }
        }
        if(l_pb.has_audit_profile())
        {
                if(m_profile_audit)
                {
                        delete m_profile_audit;
                        m_profile_audit = NULL;
                }
                m_profile_audit = new profile(m_engine);
                m_profile_audit->m_action = l_action;
                int32_t l_s;
                l_s = m_profile_audit->load(&(l_pb.audit_profile()));
                if(l_s != WAFLZ_STATUS_OK)
                {
                        WAFLZ_PERROR(m_err_msg, "%s", m_profile_audit->get_err_msg());
                        return WAFLZ_STATUS_ERROR;
                }
        }
        // -----------------------------------------------------------
        //                 P R O D   P R O F I L E
        // -----------------------------------------------------------
        VERIFY_HAS(l_pb, prod_profile_action);
        l_val = l_pb.prod_profile_action();
        if(l_val == "alert")
        {
                l_action = waflz_pb::enforcement_type_t_ALERT;
        }
        else if(l_val == "block")
        {
                l_action = waflz_pb::enforcement_type_t_BLOCK_REQUEST;
        }
        else
        {
                //ERROR("Invalid audit_profile_action value: %.*s", SUBBUF_FORMAT((*m_json)["audit_profile_action"].str()));
                WAFLZ_PERROR(m_err_msg, "Invalid prod_profile_action value: %s", l_val.c_str());
                return WAFLZ_STATUS_ERROR;
        }
        if(l_pb.prod_profile_enforcements_size())
        {
                int32_t l_s;
                l_s = waf_config_check_enf_array(m_err_msg,
                                                 l_action,
                                                 l_pb.prod_profile_enforcements(),
                                                 "prod_profile_enforcements");
                if(l_s != WAFLZ_STATUS_OK)
                {
                        return WAFLZ_STATUS_ERROR;
                }
        }
        if(l_pb.has_prod_profile())
        {
                if(m_profile_prod)
                {
                        delete m_profile_prod;
                        m_profile_prod = NULL;
                }
                m_profile_prod = new profile(m_engine);
                m_profile_prod->m_action = l_action;
                int32_t l_s;
                l_s = m_profile_prod->load(&(l_pb.prod_profile()));
                if(l_s != WAFLZ_STATUS_OK)
                {
                        WAFLZ_PERROR(m_err_msg, "%s", m_profile_prod->get_err_msg());
                        return WAFLZ_STATUS_ERROR;
                }
        }
        return WAFLZ_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
void instance::set_event_properties(waflz_pb::event &ao_event, profile &a_profile)
{
        // -------------------------------------------------
        // set waf config specifics for logging
        // -------------------------------------------------
        ao_event.set_waf_instance_id(m_id);
        ao_event.set_waf_instance_name(m_name);
        ao_event.set_waf_profile_action(a_profile.get_action());
        if (!a_profile.get_resp_header_name().empty())
        {
                ao_event.set_response_header_name(a_profile.get_resp_header_name());
        }
}
//! ----------------------------------------------------------------------------
//! \details TODO
//! \return  TODO
//! \param   TODO
//! ----------------------------------------------------------------------------
int32_t instance::process(const waflz_pb::enforcement **ao_enf,
                          waflz_pb::event **ao_audit_event,
                          waflz_pb::event **ao_prod_event,
                          void *a_ctx,
                          part_mk_t a_part_mk,
                          const rqst_ctx_callbacks *a_callbacks,
                          rqst_ctx **ao_rqst_ctx)
{
        // sanity checking...
        if(!ao_enf ||
           !ao_audit_event ||
           !ao_prod_event)
        {
                return WAFLZ_STATUS_ERROR;
        }
        *ao_enf = NULL;
        *ao_audit_event = NULL;
        *ao_prod_event = NULL;
        int32_t l_s;
        rqst_ctx *l_rqst_ctx = NULL;
        // TODO -fix args!!!
        //l_rqst_ctx = new rqst_ctx(a_ctx, l_body_size_max, m_waf->get_parse_json());
        l_rqst_ctx = new rqst_ctx(a_ctx, DEFAULT_BODY_SIZE_MAX, a_callbacks);
        // -------------------------------------------------
        // *************************************************
        //                    A U D I T
        // *************************************************
        // -------------------------------------------------
        if(!m_profile_audit)
        {
                goto process_prod;
        }
        l_s = m_profile_audit->process(ao_audit_event, a_ctx, a_part_mk, &l_rqst_ctx);
        if(l_s != WAFLZ_STATUS_OK)
        {
                if(!ao_rqst_ctx && l_rqst_ctx) { delete l_rqst_ctx; l_rqst_ctx = NULL; }
                return WAFLZ_STATUS_ERROR;
        }
        // check for whitelist
        if(l_rqst_ctx && l_rqst_ctx->m_wl) { l_rqst_ctx->m_wl_audit = true; }
        // -------------------------------------------------
        // set properties
        // -------------------------------------------------
        if(*ao_audit_event)
        {
                set_event_properties(**ao_audit_event, *m_profile_audit);
        }
        // -------------------------------------------------
        // reset phase 1
        // -------------------------------------------------
        if(l_rqst_ctx)
        {
                l_s = l_rqst_ctx->reset_phase_1();
        }
        // -------------------------------------------------
        // *************************************************
        //                     P R O D
        // *************************************************
        // -------------------------------------------------
process_prod:
        if(!m_profile_prod)
        {
                goto done;
        }
        l_s = m_profile_prod->process(ao_prod_event, a_ctx, a_part_mk, &l_rqst_ctx);
        if(l_s != WAFLZ_STATUS_OK)
        {
                if(!ao_rqst_ctx && l_rqst_ctx) { delete l_rqst_ctx; l_rqst_ctx = NULL; }
                return WAFLZ_STATUS_ERROR;
        }
        // check for whitelist
        if(l_rqst_ctx && l_rqst_ctx->m_wl) { l_rqst_ctx->m_wl_prod = true; }
        // -------------------------------------------------
        // set properties
        // -------------------------------------------------
        if(*ao_prod_event)
        {
                set_event_properties(**ao_prod_event, *m_profile_prod);
                // -----------------------------------------
                // copy out enforcement if exist
                // -----------------------------------------
                if(m_pb->prod_profile_enforcements_size())
                {
                        *ao_enf = &(m_pb->prod_profile_enforcements(0));
                }
        }
done:
        if(ao_rqst_ctx)
        {
                *ao_rqst_ctx = l_rqst_ctx;
        }
        else
        {
                if(l_rqst_ctx) { delete l_rqst_ctx; l_rqst_ctx = NULL; }
        }
        return WAFLZ_STATUS_OK;
}
}
