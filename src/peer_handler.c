/*
 * peer_handler.c
 *
 *  Created on: May 7, 2015
 *      Author: pchero
 */

#define _GNU_SOURCE


#include <stdbool.h>
#include <stdio.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include "common.h"
#include "db_handler.h"
#include "memdb_handler.h"
#include "slog.h"
#include "agent_handler.h"
#include "olive_result.h"
#include "htp_handler.h"
#include "peer_handler.h"

static json_t*  get_peerdb_all(void);
static json_t*  get_peerdb(const char* peer_name);
static bool     delete_peerdb(const json_t* j_peer);
static bool     create_peerdb(const json_t* j_peer);
static bool     update_peerdb(const json_t* j_peer);


/**
 * Get all peer info from    memdb.
 * @return
 */
json_t* get_peer_all(void)
{
    char* sql;
    int ret;
    memdb_res* mem_res;
    json_t* j_res;
    json_t* j_tmp;

    ret = asprintf(&sql, "select * from peer;");

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get peer info from memdb.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_res, j_tmp);
        json_decref(j_tmp);

    }
    memdb_free(mem_res);

    return j_res;

}

/**
 * Get all peer info from db.
 * Get all peer managed by olive.
 * @return
 */
static json_t* get_peerdb_all(void)
{
    char* sql;
    int ret;
    db_ctx_t* db_res;
    json_t* j_res;
    json_t* j_tmp;

    ret = asprintf(&sql, "select * from peer where tm_delete is null;");

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get peerdb info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = db_get_record(db_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_res, j_tmp);
        json_decref(j_tmp);

    }
    db_free(db_res);

    return j_res;
}


static bool create_peerdb(const json_t* j_peer)
{
    int ret;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_peer);

    // utc create timestamp
    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_create", json_string(cur_time));
    free(cur_time);

    ret = db_insert("peer", j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create campaign.");
        return false;
    }
    json_decref(j_tmp);

    return true;
}

/**
 * Update peerdb info.
 * @param j_peer
 * @return
 */
static bool update_peerdb(const json_t* j_peer)
{
    char* sql;
    char* tmp;
    char* cur_time;
    int ret;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_peer);

    // set update time.
    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_update_property", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_tmp);
    json_decref(j_tmp);

    ret = asprintf(&sql, "update peer set %s where name = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_peer, "name"))
            );
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update peerdb info.");
        return false;
    }

    return true;
}

/**
 * Get peerdb info.
 * @param peer_name
 * @param protocol
 * @return
 */
static json_t* get_peerdb(const char* peer_name)
{
    json_t* j_res;
    char* sql;
    db_ctx_t* db_res;
    unused__ int ret;

    ret = asprintf(&sql, "select * from peer where name = \"%s\" and tm_delete is null;", peer_name);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get peerdb info. peer_name[%s]", peer_name);
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;

}

/**
 * Delete peerdb info.
 * @param j_peer
 * @return
 */
static bool delete_peerdb(const json_t* j_peer)
{
    int ret;
    char* sql;
    char* cur_time;

    cur_time = get_utc_timestamp();
    ret = asprintf(&sql, "update peer set"
            " tm_delete = \"%s\","
            " delete_agent_id = \"%s\""
            " where"
            " name = \"%s\";"
            ,
            cur_time,
            json_string_value(json_object_get(j_peer, "delete_agent_id")),
            json_string_value(json_object_get(j_peer, "name"))
            );
    free(cur_time);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete peerdb info.");
        return false;
    }

    return true;
}

/**
 * Get peerdb info all API handler.
 * @return
 */
json_t* peerdb_get_all(void)
{
    json_t* j_res;
    json_t* j_tmp;

    slog(LOG_DEBUG, "peer_get_db_all");

    j_tmp = get_peerdb_all();
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get peerdb info all.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Update peerdb API handler
 * @param peer_name
 * @param j_recv
 * @param agent_id
 * @return
 */
json_t* peerdb_update(const char* peer_name, const json_t* j_recv, const char* agent_id)
{
    unused__ int ret;
    json_t* j_res;
    json_t* j_tmp;
    char* cur_time;

    j_tmp = json_deep_copy(j_recv);

    // set info
    json_object_set_new(j_tmp, "update_property_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "name", json_string(peer_name));

    // update
    ret = update_peerdb(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update peerdb. peer_name[%s], agent_id[%s]",
                peer_name, agent_id);
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get updated info.
    j_tmp = get_peerdb(peer_name);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get created peerdb info. peer_name[%s]", peer_name);
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // create result.
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Delete peerdb API handler.
 * @param peer_name
 * @param agent_id
 * @return
 */
json_t* peerdb_delete(const char* peer_name, const char* agent_id)
{
    int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_object();

    json_object_set_new(j_tmp, "delete_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "name", json_string(peer_name));

    ret = delete_peerdb(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete peerdb info. peer_name[%s], agent_id[%s]", peer_name, agent_id);
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, json_null());

    return j_res;
}

/**
 * Create peerdb API handler.
 * @param j_camp
 * @param agent_id
 * @return
 */
json_t* peerdb_create(const json_t* j_peer, const char* agent_id)
{
    int ret;
    char* camp_uuid;
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = json_deep_copy(j_peer);

    // set create_agent_id
    json_object_set_new(j_tmp, "create_agent_id", json_string(agent_id));

    // create peerdb.
    ret = create_peerdb(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create new peerdb.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        free(camp_uuid);
        return j_res;
    }

    // get created peerdb.
    j_tmp = get_peerdb(json_string_value(json_object_get(j_peer, "name")));
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get created peerdb info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // make result
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Get peerdb API handler.
 * @param j_recv
 * @return
 */
json_t* peerdb_get(const char* name)
{
    unused__ int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = get_peerdb(name);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not find peerdb info.");
        j_res = htp_create_olive_result(OLIVE_CAMPAIGN_NOT_FOUND, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

