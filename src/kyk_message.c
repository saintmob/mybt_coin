#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "beej_pack.h"
#include "kyk_sha.h"
#include "kyk_message.h"
#include "kyk_utils.h"
#include "dbg.h"

static int kyk_copy_ptl_payload(ptl_payload* dest_pld, const ptl_payload* src_pld);
static ptl_net_addr* build_net_addr(const char* ip_src, int port);


int kyk_build_new_ptl_message(ptl_message** new_msg,
			      const char* cmd,
			      uint32_t nt_magic,
			      const ptl_payload* pld)
{
    ptl_message* msg = NULL;
    int res = -1;
    
    check(new_msg, "Failed to kyk_build_btc_new_message: new_msg is NULL");
    check(cmd, "Failed to kyk_build_btc_new_message: cmd is NULL");
    check(pld, "Failed to kyk_build_btc_new_message: pld is NULL");

    msg = calloc(1, sizeof(*msg));
    check(msg, "Failed to kyk_build_btc_new_message: msg calloc failed");    

    res = kyk_build_ptl_message(msg, cmd, nt_magic, pld);
    check(res == 0, "Failed to kyk_build_btc_new_message");

    *new_msg = msg;

    return 0;
    
error:
    if(msg) kyk_free_ptl_msg(msg);
    return -1;
}


int kyk_build_ptl_message(ptl_message* msg,
			  const char* cmd,
			  uint32_t nt_magic,
			  const ptl_payload* pld)
{
    uint256 digest;
    size_t cmd_len = 0;
    int res = -1;

    check(msg, "Failed to kyk_build_btc_message: msg is NULL");
    check(msg -> pld == NULL, "Failed to kyk_build_btc_message: msg -> pld should be NULL");
    check(pld, "Failed to kyk_build_btc_message: pld is NULL");
    
    cmd_len = strlen(cmd);
    check(cmd_len < sizeof(msg -> cmd), "Failed to kyk_build_btc_message: cmd is invalid");
    
    msg -> magic = nt_magic;
    strcpy(msg -> cmd, cmd);
    msg -> pld_len = pld -> len;

    res = kyk_copy_new_ptl_payload(&msg -> pld, pld);
    check(res == 0, "Failed to kyk_build_btc_message: kyk_copy_new_ptl_payload failed");

    res = kyk_hash256(&digest, pld -> data, pld -> len);
    check(res == 0, "Failed to kyk_build_btc_message: kyk_hash256 failed");
    memcpy(msg -> checksum, digest.data, sizeof(msg -> checksum));

    return 0;

error:

    return -1;
}

int kyk_new_ptl_payload(ptl_payload** new_pld)
{
    ptl_payload* pld = NULL;

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_new_ptl_payload: pld calloc failed");

    pld -> len = 0;
    pld -> data = NULL;

    *new_pld = pld;

    return 0;

error:

    return -1;
}

int kyk_copy_new_ptl_payload(ptl_payload** new_pld, const ptl_payload* src_pld)
{
    ptl_payload* pld = NULL;
    int res = -1;

    check(new_pld, "Failed to kyk_copy_new_ptl_payload: new_pld is NULL");
    check(src_pld, "Failed to kyk_copy_new_ptl_payload: src_pld is NULL");

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_copy_new_ptl_payload: pld calloc failed");
    
    res = kyk_copy_ptl_payload(pld, src_pld);
    check(res == 0, "Failed to kyk_copy_new_ptl_payload: kyk_copy_ptl_payload failed");

    *new_pld = pld;

    return 0;

error:
    if(pld) kyk_free_ptl_payload(pld);
    return -1;
}

int kyk_copy_ptl_payload(ptl_payload* dest_pld, const ptl_payload* src_pld)
{
    check(dest_pld, "Failed to kyk_copy_ptl_payload: dest_pld is NULL");
    check(src_pld, "Failed to kyk_copy_ptl_payload: src_pld is NULL");

    dest_pld -> len = src_pld -> len;
    dest_pld -> data = calloc(dest_pld -> len, sizeof(*dest_pld -> data));
    check(dest_pld -> data, "Failed to kyk_copy_ptl_payload: calloc failed");
    
    memcpy(dest_pld -> data, src_pld -> data, dest_pld -> len);

    return 0;
    
error:

    return -1;
}

void kyk_free_ptl_msg(ptl_message* msg)
{
    if(msg){
	if(msg -> pld){
	    kyk_free_ptl_payload(msg -> pld);
	    msg -> pld = NULL;
	}
	free(msg);
    }
}

void kyk_free_ptl_payload(ptl_payload* pld)
{
    if(pld){
	if(pld -> data){
	    free(pld -> data);
	    pld -> data = NULL;
	}
	free(pld);
    }
}

void kyk_print_ptl_message(const ptl_message* ptl_msg)
{
    ptl_msg_buf* msg_buf = NULL;

    kyk_new_seri_ptl_message(&msg_buf, ptl_msg);
    
    printf("ptl_msg -> magic:  %0x\n", ptl_msg -> magic);
    printf("ptl_msg -> cmd:     %s\n", ptl_msg -> cmd);
    printf("ptl_msg -> pld_len: %u\n", ptl_msg -> pld_len);
    kyk_print_hex("ptl_msg -> checksum", ptl_msg -> checksum, sizeof(ptl_msg -> checksum));
    kyk_print_hex("ptl_msg -> pld", ptl_msg -> pld -> data, ptl_msg -> pld_len);
    kyk_print_hex("Hex", msg_buf -> data, msg_buf -> len);
}


unsigned int pack_ptl_net_addr(unsigned char *bufp, ptl_net_addr *na)
{
    unsigned int size = 0;
    unsigned int m_size = 0;

    size = beej_pack(bufp, "<Q", na -> servs);
    m_size += size;
    bufp += size;

    size = 16;
    memcpy(bufp, na -> ipv, size);
    m_size += size;
    bufp += size;

    size = beej_pack(bufp, ">H", na -> port);
    m_size += size;
    bufp += size;

    return m_size;
}

int kyk_new_ping_entity(struct ptl_ping_entity** new_et)
{
    struct ptl_ping_entity* et = NULL;

    et = calloc(1, sizeof(*et));
    check(et, "Failed to kyk_new_ping_entity: et calloc failed");

    /* rand seeds */
    srand((unsigned)time(NULL));
    
    et -> nonce = (uint64_t)rand();
    *new_et = et;
    
    return 0;
    
error:

    return -1;
}

int kyk_deseri_new_ptl_message(ptl_message** new_ptl_msg, const uint8_t* buf, size_t checklen)
{
    const uint8_t* bufp = NULL;
    ptl_payload* pld = NULL;
    ptl_message* msg = NULL;
    
    check(buf, "Failed to kyk_deseri_new_ptl_message: buf is NULL");

    msg = calloc(1, sizeof(*msg));
    check(msg, "Failed to kyk_deseri_new_ptl_message");

    bufp = buf;

    beej_unpack(bufp, "<L", &msg -> magic);
    bufp += sizeof(msg -> magic);

    memcpy(msg -> cmd, bufp, sizeof(msg -> cmd));
    bufp += sizeof(msg -> cmd);

    beej_unpack(bufp, "<L", &msg -> pld_len);
    bufp += sizeof(msg -> pld_len);

    memcpy(msg -> checksum, bufp, sizeof(msg -> checksum));
    bufp += sizeof(msg -> checksum);

    msg -> pld = calloc(1, sizeof(*msg -> pld));
    check(msg -> pld, "Failed to kyk_deseri_new_ptl_message: calloc failed");

    pld = msg -> pld;

    pld -> len = msg -> pld_len;
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_deseri_new_ptl_message: calloc failed");

    memcpy(pld -> data, bufp, pld -> len);
    bufp += pld -> len;

    if(checklen > 0){
	check(checklen == (size_t)(bufp - buf), "Failed to kyk_deseri_new_ptl_message");
    }

    *new_ptl_msg = msg;
  
    return 0;

error:
    if(msg) kyk_free_ptl_msg(msg);
    return -1;
}

int kyk_new_seri_ptl_message(ptl_msg_buf** new_msg_buf, const ptl_message* msg)
{
    ptl_msg_buf* msg_buf = NULL;
    size_t msg_size = 0;
    int res = -1;

    res = kyk_get_ptl_msg_size(msg, &msg_size);
    check(res == 0, "Failed to kyk_new_seri_ptl_message: kyk_get_ptl_msg_size failed");
    
    res = kyk_new_msg_buf(&msg_buf, msg_size);
    check(res == 0, "Failed to kyk_new_seri_ptl_message: kyk_new_msg_buf failed");

    res = kyk_seri_ptl_message(msg_buf, msg);
    check(res == 0, "Failed to kyk_new_seri_ptl_message: kyk_seri_ptl_message failed");

    *new_msg_buf = msg_buf;

    return 0;
    
error:
    if(msg_buf) kyk_free_ptl_msg_buf(msg_buf);
    return -1;
}

int kyk_new_msg_buf(ptl_msg_buf** new_msg_buf, uint32_t len)
{
    ptl_msg_buf* msg_buf = NULL;
    
    check(len > 0, "Failed to kyk_new_msg_buf: len should be > 0");

    msg_buf = calloc(len, sizeof(*msg_buf));
    check(msg_buf, "Failed to kyk_new_msg_buf: msg_buf calloc failed");

    msg_buf -> len = len;
    msg_buf -> data = calloc(len, sizeof(*msg_buf -> data));
    check(msg_buf -> data, "Failed to kyk_new_msg_buf: msg_buf -> data calloc failed");

    *new_msg_buf = msg_buf;

    return 0;
    
error:
    if(msg_buf) kyk_free_ptl_msg_buf(msg_buf);
    return -1;
}

void kyk_free_ptl_msg_buf(ptl_msg_buf* msg_buf)
{
    if(msg_buf){
	if(msg_buf -> data){
	    free(msg_buf -> data);
	    msg_buf -> data = NULL;
	}

	free(msg_buf);
    }
}

int kyk_get_ptl_msg_size(const ptl_message* msg, size_t* msg_size)
{
    size_t len = 0;
    
    check(msg, "Failed to kyk_get_ptl_msg_size: msg is NULL");
    check(msg_size, "Failed to kyk_get_ptl_msg_size: msg_size is NULL");

    len += sizeof(msg -> magic);
    len += sizeof(msg -> cmd);
    len += sizeof(msg -> pld_len);
    len += sizeof(msg -> checksum);
    len += msg -> pld_len;

    *msg_size = len;

    return 0;
    
error:

    return -1;
}

/*
** Message structure
**
** Field Size|	Description	Data type	Comments
** ----------+----------------------------------------------------------------------------------------------------------------------------------------------
** 4	     |  magic	        uint32_t	Magic value indicating message origin network, and used to seek to next message when stream state is unknown
** ----------+----------------------------------------------------------------------------------------------------------------------------------------------
** 12	     | command	        char[12]	ASCII string identifying the packet content, NULL padded (non-NULL padding results in packet rejected)
** ----------+----------------------------------------------------------------------------------------------------------------------------------------------
** 4	     | length	        uint32_t	Length of payload in number of bytes
** ----------+----------------------------------------------------------------------------------------------------------------------------------------------
** 4	     | checksum	        uint32_t	First 4 bytes of sha256(sha256(payload))
** ----------+----------------------------------------------------------------------------------------------------------------------------------------------
** ?	     | payload	        uchar[]	        The actual data
** ---------------------------------------------------------------------------------------------------------------------------------------------------------
*/
int kyk_seri_ptl_message(ptl_msg_buf* msg_buf, const ptl_message* msg)
{
    unsigned char *buf = NULL;
    size_t len = 0;

    check(msg_buf, "Failed to kyk_seri_ptl_message: msg_buf is NULL");
    check(msg, "Failed to kyk_seri_ptl_message: ptl_msg is NULL");
    check(msg -> pld, "Failed to kyk_seri_ptl_message: msg -> pld is NULL");
    check(msg -> pld_len == msg -> pld -> len, "Failed to kyk_seri_ptl_message: invalid msg -> pld_len");

    buf = msg_buf -> data;
    msg_buf -> len = 0;
    len = beej_pack(buf, "<L", msg -> magic);
    msg_buf -> len += len;
    buf += len;

    len = sizeof(msg -> cmd);
    memcpy(buf, msg -> cmd, len);
    msg_buf -> len += len;
    buf += len;
    
    len = beej_pack(buf, "<L", msg -> pld_len);
    msg_buf -> len += len;
    buf += len;

    len = sizeof(msg -> checksum);
    memcpy(buf, msg -> checksum, len);
    msg_buf -> len += len;
    buf += len;

    len = msg -> pld_len;
    memcpy(buf, msg -> pld -> data, len);
    msg_buf -> len += len;
    buf += len;

    return 0;

error:

    return -1;
}



/* build payload */

/* The ping message is sent primarily to confirm that the TCP/IP connection is still valid. An error in transmission is presumed to be a closed connection and the address is removed as a current peer. */
/* Payload: */
/* Field Size	Description	Data type	Comments     */
/* 8	        nonce	        uint64_t	random nonce */

int kyk_build_new_ping_payload(ptl_payload** new_pld, const struct ptl_ping_entity* et)
{
    ptl_payload* pld = NULL;
    int res = -1;

    res = kyk_new_ptl_payload(&pld);
    check(res == 0, "Failed to kyk_build_new_ping_payload: kyk_new_ptl_payload failed");

    pld -> len = sizeof(et -> nonce);
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_build_new_ping_payload: pld -> data calloc failed");
    beej_pack(pld -> data, "<Q", et -> nonce);

    *new_pld = pld;

    return 0;

error:
    if(pld) kyk_free_ptl_payload(pld);
    return -1;
}

int kyk_build_new_pong_payload(ptl_payload** new_pld, uint64_t nonce)
{
    ptl_payload* pld = NULL;
    int res = -1;

    res = kyk_new_ptl_payload(&pld);
    check(res == 0, "Failed to kyk_build_new_pong_payload: kyk_new_ptl_payload failed");

    pld -> len = sizeof(nonce);
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_build_new_pong_payload: pld -> data calloc failed");
    beej_pack(pld -> data, "<Q", nonce);

    *new_pld = pld;

    return 0;
    
error:

    return -1;
}


int kyk_build_new_version_entity(ptl_ver_entity** new_ver,
				 int32_t vers,
				 const char* ip_src,
				 int port,
				 uint64_t nonce,
				 const char* uagent,
				 uint8_t ua_len,
				 int32_t start_height)
{
    ptl_ver_entity* ver = NULL;

    check(new_ver, "Failed to kyk_build_new_version_entity: new_ver is NULL");
    check(uagent, "Failed to kyk_build_new_version_entity: uagent is NULL");

    ver = calloc(1, sizeof(*ver));
    check(ver, "Failed to kyk_build_new_version_entity: calloc failed");
    
    /* example vers 70014 */
    ver -> vers = vers;
    
    ver -> servs = NODE_NETWORK;
    ver -> ttamp = (int64_t)time(NULL);
    ver -> addr_recv_ptr = build_net_addr(ip_src, port);
    ver -> addr_from_ptr = build_net_addr(ip_src, port);
    
    /* example nonce: 0 */
    ver -> nonce = nonce;
    ver -> ua_len = ua_len;
    ver -> uagent = calloc(ua_len + 1, sizeof(*ver -> uagent));
    /* uagent example: /Satoshi:0.9.2.1/ */
    check(ver -> uagent, "Failed to kyk_build_new_version_entity: calloc failed");
    memcpy(ver -> uagent, uagent, ver -> ua_len + 1);
    
    /* example start height: 329167 */
    ver -> start_height = start_height;
    ver -> relay = 0;

    *new_ver = ver;

    return 0;

error:

    return -1;
}

/* example ip_src: "::ffff:127.0.0.1" */
/* bitcoin node default port is 8333 */
static ptl_net_addr* build_net_addr(const char* ip_src, int port)
{
    ptl_net_addr *na_ptr;
    int s, domain;
    
    domain = AF_INET6;

    na_ptr = malloc(sizeof *na_ptr);
    na_ptr -> servs = 1;
    na_ptr -> port = port;
    s = inet_pton(domain, ip_src, na_ptr -> ipv);
    if (s <= 0) {
	if (s == 0)
	    fprintf(stderr, "Not in presentation format");
	else
	    perror("inet_pton");
	exit(EXIT_FAILURE);
    }

    return na_ptr;
}


int kyk_new_seri_ver_entity_to_pld(ptl_ver_entity* ver, ptl_payload** new_pld)
{
    ptl_payload* pld = NULL;
    int res = -1;
    
    check(ver, "Failed to kyk_new_seri_ver_entity_to_pld: ver is NULL");
    check(new_pld, "Failed to kyk_new_seri_ver_entity_to_pld: new_pld is NULL");

    res = kyk_new_ptl_payload(&pld);
    check(res == 0, "Failed to kyk_new_seri_ver_entity_to_pld: kyk_new_ptl_payload failed");

    res = kyk_get_ptl_ver_entity_size(ver, (size_t*)&pld -> len);
    check(res == 0, "Failed to kyk_new_seri_ver_entity_to_pld");

    pld -> data = calloc(pld -> len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_new_seri_ver_entity_to_pld: calloc failed");

    kyk_seri_version_entity_to_pld(ver, pld);

    *new_pld = pld;

    return 0;

error:

    return -1;
}

int kyk_get_ptl_ver_entity_size(ptl_ver_entity* ver, size_t* entity_size)
{
    size_t total_len = 0;
    size_t len = 0;
    int res = -1;
    
    check(ver, "Failed to kyk_get_ptl_ver_entity_size: ver is NULL");

    total_len += sizeof(ver -> vers);
    total_len += sizeof(ver -> servs);
    total_len += sizeof(ver -> ttamp);
    
    res = kyk_get_ptl_net_addr_size(ver -> addr_recv_ptr, &len);
    check(res == 0, "Failed to kyk_get_ptl_ver_entity_size");
    total_len += len;

    res = kyk_get_ptl_net_addr_size(ver -> addr_from_ptr, &len);
    check(res == 0, "Failed to kyk_get_ptl_ver_entity_size");
    total_len += len;
    
    total_len += sizeof(ver -> nonce);
    total_len += sizeof(ver -> ua_len);
    if(ver -> ua_len > 0){
	total_len += ver -> ua_len;
    } else {
	total_len += 1;
    }
    total_len += sizeof(ver -> start_height);
    total_len += sizeof(ver -> relay);

    *entity_size = total_len;

    return 0;
    
error:

    return -1;
}

int kyk_get_ptl_net_addr_size(ptl_net_addr* net_addr, size_t* net_addr_size)
{
    size_t total_len = 0;
    
    check(net_addr, "Failed to kyk_get_ptl_net_addr_size: net_addr is NULL");

    total_len += sizeof(net_addr -> servs);
    total_len += sizeof(net_addr -> ipv);
    total_len += sizeof(net_addr -> port);

    *net_addr_size = total_len;

    return 0;
    
error:

    return -1;
}

int kyk_seri_version_entity_to_pld(ptl_ver_entity* ver, ptl_payload* pld)
{
    unsigned int len;
    unsigned char *bufp = pld -> data;

    check(pld, "Failed to kyk_seri_version_entity_to_pld: pld is NULL");
    check(pld -> data, "Failed to kyk_seri_version_entity_to_pld: pld -> data is NULL");
    
    len = beej_pack(bufp, "<l", ver -> vers);
    bufp += len;

    len = beej_pack(bufp, "<Q", ver -> servs);
    bufp += len;

    len = beej_pack(bufp, "<q", ver -> ttamp);
    bufp += len;

    len = pack_ptl_net_addr(bufp, ver -> addr_recv_ptr);
    bufp += len;

    len = pack_ptl_net_addr(bufp, ver -> addr_from_ptr);
    bufp += len;

    len = beej_pack(bufp, "<Q", ver -> nonce);
    bufp += len;

    *bufp = ver -> ua_len;
    bufp += sizeof(ver -> ua_len);

    if(ver -> ua_len > 0){
	memcpy(bufp, ver -> uagent, ver -> ua_len);
	bufp += ver -> ua_len;
    } else {
	*bufp = 0x00;
	bufp += 1;
    }

    len = beej_pack(bufp, "<l", ver -> start_height);
    bufp += len;

    *bufp = ver -> relay;
    
    return 0;

error:

    return -1;

}

int kyk_deseri_new_version_entity(ptl_ver_entity** new_ver_entity, uint8_t* buf, size_t* checknum)
{
    ptl_ver_entity* ver_entity = NULL;
    size_t len = 0;
    size_t total_len = 0;
    uint8_t* bufp = NULL;

    check(buf, "Failed to kyk_deseri_new_version_entity: buf is NULL");

    bufp = buf;

    ver_entity = calloc(1, sizeof(*ver_entity));
    check(ver_entity, "Failed to kyk_deseri_new_version_entity: ver_entity calloc failed");

    beej_unpack(bufp, "<l", &ver_entity -> vers);
    len = sizeof(ver_entity -> vers);
    total_len += len;
    bufp += len;
    
    beej_unpack(bufp, "<Q", &ver_entity -> servs);
    len = sizeof(ver_entity -> servs);
    total_len += len;
    bufp += len;

    beej_unpack(bufp, "<q", &ver_entity -> ttamp);
    len = sizeof(ver_entity -> ttamp);
    total_len += len;
    bufp += len;

    kyk_deseri_new_net_addr(&ver_entity -> addr_recv_ptr, bufp, &len);
    total_len += len;
    bufp += len;

    kyk_deseri_new_net_addr(&ver_entity -> addr_from_ptr, bufp, &len);
    total_len += len;
    bufp += len;

    beej_unpack(bufp, "<Q", &ver_entity -> nonce);
    len = sizeof(ver_entity -> nonce);
    total_len += len;
    bufp += len;

    ver_entity -> ua_len = *bufp;
    len = sizeof(ver_entity -> ua_len);
    total_len += len;
    bufp += len;

    ver_entity -> uagent = calloc(ver_entity -> ua_len + 1, sizeof(*ver_entity -> uagent));
    check(ver_entity -> uagent, "Failed to kyk_deseri_new_version_entity");
    memcpy(ver_entity -> uagent, bufp, ver_entity -> ua_len);
    total_len += ver_entity -> ua_len;
    bufp += ver_entity -> ua_len;

    beej_unpack(bufp, "<l", &ver_entity -> start_height);
    len = sizeof(ver_entity -> start_height);
    total_len += len;
    bufp += len;

    ver_entity -> relay = *bufp;
    len = sizeof(ver_entity -> relay);
    total_len += len;
    bufp += len;


    if(checknum){
	*checknum = total_len;
    }

    *new_ver_entity = ver_entity;
    
    return 0;

error:

    return -1;
}

int kyk_deseri_new_net_addr(ptl_net_addr** new_net_addr, uint8_t* buf, size_t* checknum)
{
    ptl_net_addr* net_addr = NULL;
    size_t len = 0;
    size_t total_len = 0;
    uint8_t* bufp = NULL;

    check(buf, "Failed to kyk_deseri_new_net_addr: buf is NULL");

    net_addr = calloc(1, sizeof(*net_addr));
    check(net_addr, "Failed to kyk_deseri_new_net_addr: net_addr calloc failed");

    bufp = buf;

    beej_unpack(bufp, "<Q", &net_addr -> servs);
    len = sizeof(net_addr -> servs);
    total_len += len;
    bufp += len;

    memcpy(net_addr -> ipv, bufp, sizeof(net_addr -> ipv));
    len = sizeof(net_addr -> ipv);
    total_len += len;
    bufp += len;

    beej_unpack(bufp, ">H", &net_addr -> port);
    len = sizeof(net_addr -> port);
    total_len += len;
    bufp += len;

    if(checknum){
	*checknum = total_len;
    }

    *new_net_addr = net_addr;
    

    return 0;

error:

    return -1;
}

void kyk_print_ptl_version_entity(ptl_ver_entity* ver)
{
    printf("vers: %d\n", ver -> vers);
    printf("ttamp: %lld\n", ver -> ttamp);
    printf("recv addr servs: %llu\n", ver -> addr_recv_ptr -> servs);
    kyk_print_hex("recv addr ip", ver -> addr_recv_ptr -> ipv, 16);
    printf("recv addr port: %u\n", ver -> addr_recv_ptr -> port);
    printf("from addr servs: %llu\n", ver -> addr_from_ptr -> servs);
    kyk_print_hex("from addr ip", ver -> addr_from_ptr -> ipv, 16);
    printf("from addr port: %u\n", ver -> addr_from_ptr -> port);
    printf("nonce: %llu\n", ver -> nonce);
    printf("ua_len: %d\n", ver -> ua_len);
    printf("uagent: %s\n", ver -> uagent);
    printf("start_height: %u\n", ver -> start_height);
}

void kyk_free_ptl_gethder_entity(ptl_gethder_entity* entity)
{
    if(entity){
	if(entity -> locator_hashes){
	    free(entity -> locator_hashes);
	    entity -> locator_hashes = NULL;
	}

	free(entity);
    }
}

int kyk_build_new_getheaders_entity(ptl_gethder_entity** new_entity,
				    uint32_t version)
				    
{
    ptl_gethder_entity* entity = NULL;
    varint_t i = 0;
    uint256* uhash = NULL;

    check(new_entity, "Failed to kyk_build_new_getheaders_entity: new_entity is NULL");

    entity = calloc(1, sizeof(*entity));
    check(entity, "Failed to kyk_build_new_getheaders_entity: entity calloc failed");

    entity -> version = version;
    entity -> hash_count = 1;
    entity -> locator_hashes = calloc(entity -> hash_count, sizeof(*entity -> locator_hashes));
    check(entity -> locator_hashes, "Failed to kyk_build_new_getheaders_entity: calloc failed");

    for(i = 0; i < entity -> hash_count; i++){
	uhash = entity -> locator_hashes + i;
	memset(uhash -> data, 0, sizeof(uhash -> data));
    }

    uhash = &entity -> hash_stop;
    memset(uhash -> data, 0, sizeof(uhash -> data));

    uhash = NULL;

    *new_entity = entity;

    return 0;

error:
    if(entity) kyk_free_ptl_gethder_entity(entity);
    return -1;
}

int kyk_new_seri_gethder_entity_to_pld(ptl_gethder_entity* et, ptl_payload** new_pld)
{
    ptl_payload* pld = NULL;
    size_t pld_len = 0;
    size_t len = 0;
    varint_t i = 0;
    uint256* h = NULL;
    uint8_t* bufp = NULL;
    
    check(et, "Failed to kyk_new_seri_gethder_entity_to_pld: et is NULL");

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_new_seri_gethder_entity_to_pld: pld calloc failed");

    kyk_get_gethder_entity_size(et, &pld_len);
    check(pld_len > 0, "Failed to kyk_new_seri_gethder_entity_to_pld");

    pld -> len = pld_len;
    pld -> data = calloc(pld_len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_new_seri_gethder_entity_to_pld");

    bufp = pld -> data;

    len = beej_pack(bufp, "<L", et -> version);
    bufp += len;

    len = kyk_pack_varint(bufp, et -> hash_count);
    bufp += len;

    for(i = 0; i < et -> hash_count; i++){
	h = et -> locator_hashes + i;
	memcpy(bufp, h -> data, sizeof(h -> data));
	bufp += sizeof(h -> data);
    }

    h = &et -> hash_stop;
    memcpy(bufp, h -> data, sizeof(h -> data));
    bufp += sizeof(h -> data);

    *new_pld = pld;

    return 0;
    
error:
    
    return -1;
}

int kyk_get_gethder_entity_size(ptl_gethder_entity* et, size_t* elen)
{
    size_t total_len = 0;
    check(et, "Failed to kyk_get_gethder_entity_size: et is NULL");

    total_len += sizeof(et -> version);
    total_len += get_varint_size(et -> hash_count);
    total_len += et -> hash_count * sizeof(uint256);
    total_len += sizeof(uint256);

    *elen = total_len;

    return 0;

error:

    return -1;
}


int kyk_seri_hd_chain_to_new_pld(ptl_payload** new_pld, const struct kyk_blk_hd_chain* hd_chain)
{
    ptl_payload* pld = NULL;
    struct kyk_blk_header* hd = NULL;
    uint8_t* bufp = NULL;
    size_t pld_len = 0;
    size_t len = 0;
    size_t i = 0;
    
    check(hd_chain, "Failed to kyk_seri_hd_chain_to_new_pld: hd_chain is NULL");
    check(hd_chain -> len > 0, "Failed to kyk_seri_hd_chain_to_new_pld: hd_chain is NULL");

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_seri_hd_chain_to_new_pld: calloc failed");

    kyk_get_headers_pld_len(hd_chain, &pld_len);

    pld -> len = pld_len;
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_seri_hd_chain_to_new_pld: calloc failed");

    bufp = pld -> data;

    len = kyk_pack_varint(bufp, (varint_t)hd_chain -> len);
    bufp += len;

    for(i = 0; i < hd_chain -> len; i++){
	hd = hd_chain -> hd_list + i;
	len = kyk_seri_blk_hd(bufp, hd);
	bufp += len;
	*bufp = 0;
	bufp += 1;
    }

    *new_pld = pld;
    
    return 0;

error:

    return -1;
    
}

int kyk_get_headers_pld_len(const struct kyk_blk_hd_chain* hd_chain, size_t* pld_len)
{
    size_t len = 0;
    size_t total_len = 0;
    varint_t count = 0;

    check(hd_chain, "Failed to kyk_get_headers_pld_len: hd_chain is NULL");

    count = hd_chain -> len;

    len = get_varint_size(count);
    total_len += len;

    /* 1 byte for txn_count    var_int	Number of transaction entries, this value is always 0 */
    len = KYK_BLK_HD_LEN + 1;

    total_len += hd_chain -> len * len;

    *pld_len = total_len;

    return 0;

error:

    return -1;
}

int kyk_deseri_headers_msg_to_new_hd_chain(ptl_message* msg, struct kyk_blk_hd_chain** new_hd_chain)
{
    struct kyk_blk_hd_chain* hd_chain = NULL;
    struct kyk_blk_header* hd = NULL;
    uint8_t* bufp = NULL;
    size_t len = 0;
    size_t i = 0;
    varint_t count = 0;
    int res = -1;
    
    check(msg, "Failed to kyk_deseri_headers_msg_to_new_hd_chain: msg is NULL");

    hd_chain = calloc(1, sizeof(*hd_chain));
    check(hd_chain, "Failed to kyk_deseri_headers_msg_to_new_hd_chain: calloc failed");

    bufp = msg -> pld -> data;

    len = kyk_unpack_varint(bufp, &count);
    bufp += len;

    hd_chain -> len = count;

    hd_chain -> hd_list = calloc(hd_chain -> len, sizeof(*hd_chain -> hd_list));
    check(hd_chain -> hd_list, "Failed to kyk_deseri_headers_msg_to_new_hd_chain: calloc failed");

    for(i = 0; i < hd_chain -> len; i++){
	hd = hd_chain -> hd_list + i;
	res = kyk_deseri_blk_header(hd, bufp, &len);
	check(res == 0, "Failed to kyk_deseri_headers_msg_to_new_hd_chain: kyk_deseri_blk_header failed");
	bufp += len;
	bufp += 1;
    }

    *new_hd_chain = hd_chain;

    return 0;
    
error:

    return -1;
}

int kyk_seri_ptl_inv_list_to_new_pld(ptl_payload** new_pld,
				     const struct ptl_inv* inv_list,
				     varint_t inv_count)
{
    ptl_payload* pld = NULL;
    const struct ptl_inv* inv = NULL;
    uint8_t* bufp = NULL;
    size_t len = 0;
    varint_t i = 0;

    check(inv_list, "Failed to kyk_seri_ptl_inv_list_to_new_pld: inv_list is NULL");

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_seri_ptl_inv_list_to_new_pld: calloc failed");

    pld -> len = get_varint_size(inv_count) + inv_count * sizeof(struct ptl_inv);
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));

    check(pld -> data, "Failed to kyk_seri_ptl_inv_list_to_new_pld: calloc failed");

    bufp = pld -> data;
    len = kyk_pack_varint(bufp, inv_count);
    bufp += len;

    for(i = 0; i < inv_count; i++){
	inv = inv_list + i;
	kyk_seri_ptl_inv(bufp, inv, &len);
	bufp += len;
    }

    *new_pld = pld;

    return 0;

error:

    return -1;
}

int kyk_seri_ptl_inv(uint8_t* buf,
		     const struct ptl_inv* inv,
		     size_t* checknum)
{
    uint8_t* bufp = NULL;
    
    check(buf, "Failed to kyk_seri_ptl_inv: buf is NULL");
    check(inv, "Failed to kyk_seri_ptl_inv: inv is NULL");

    bufp = buf;

    beej_pack(bufp, "<L", inv -> type);
    bufp += sizeof(inv -> type);

    memcpy(bufp, inv -> hash, sizeof(inv -> hash));
    bufp += sizeof(inv -> hash);

    if(checknum){
	*checknum = bufp - buf;
    }

    return 0;
    
error:

    return -1;
}

void kyk_print_inv_list(const struct ptl_inv* inv_list, varint_t inv_count)
{
    const struct ptl_inv* inv = NULL;
    varint_t i = 0;

    for(i = 0; i < inv_count; i++){
	inv = inv_list + i;
	kyk_print_inv(inv);
    }
}

void kyk_print_inv(const struct ptl_inv* inv)
{
    switch(inv -> type){
    case PTL_INV_ERROR:
	printf("inv -> type: PTL_INV_ERROR\n");
	break;
    case PTL_INV_MSG_TX:
	printf("inv -> type: PTL_INV_MSG_TX\n");
	break;
    case PTL_INV_MSG_BLOCK:
	printf("inv -> type: PTL_INV_MSG_BLOCK\n");
	break;
    case PTL_INV_MSG_FILTERED_BLOCK:
	printf("inv -> type: PTL_INV_MSG_FILTERED_BLOCK\n");
	break;
    case PTL_INV_MSG_CMPCT_BLOCK:
	printf("inv -> type: PTL_INV_MSG_CMPCT_BLOCK\n");
	break;
    default:
	printf("inv -> type: invalid type\n");
	break;
    }

    kyk_print_hex("inv -> hash", (uint8_t*)inv -> hash, sizeof(inv -> hash));
}

int kyk_deseri_new_ptl_inv_list(const uint8_t* buf,
				struct ptl_inv** new_inv_list,
				varint_t* inv_count)
{
    struct ptl_inv* inv_list = NULL;
    struct ptl_inv* inv = NULL;
    size_t len = 0;
    varint_t count = 0;
    varint_t i = 0;
    const uint8_t* bufp = NULL;
    
    check(buf, "Failed to kyk_deseri_new_ptl_inv_list: buf is NULL");

    bufp = buf;

    len = kyk_unpack_varint(bufp, &count);
    bufp += len;

    inv_list = calloc(count, sizeof(*inv_list));
    check(inv_list, "Failed to kyk_deseri_new_ptl_inv_list: calloc failed");

    for(i = 0; i < count; i++){
	inv = inv_list + i;
	kyk_deseri_ptl_inv(bufp, inv, &len);
	check(len > 0, "Failed to kyk_deseri_new_ptl_inv_list: kyk_deseri_ptl_inv failed");
	bufp += len;
    }
    
    *new_inv_list = inv_list;
    *inv_count = count;

    return 0;
    
error:

    return -1;
}

int kyk_deseri_ptl_inv(const uint8_t* buf, struct ptl_inv* inv, size_t* checknum)
{
    const uint8_t* bufp = NULL;
    
    check(buf, "Failed to kyk_deseri_ptl_inv: buf is NULL");
    check(inv, "Failed to kyk_deseri_ptl_inv: inv is NULL");

    bufp = buf;

    beej_unpack(bufp, "<L", &inv -> type);
    bufp += sizeof(inv -> type);

    memcpy(inv -> hash, bufp, sizeof(inv -> hash));
    bufp += sizeof(inv -> hash);

    *checknum = bufp - buf;

    return 0;
    
error:

    return -1;
}

int kyk_hd_chain_to_inv_list(const struct kyk_blk_hd_chain* hd_chain,
			     uint32_t type,
			     struct ptl_inv** new_inv_list,
			     varint_t* inv_count)
			     
{
    struct ptl_inv* inv_list = NULL;
    struct ptl_inv* inv = NULL;
    const struct kyk_blk_header* hd = NULL;
    uint8_t digest[32];
    size_t i = 0;

    check(hd_chain, "Failed to kyk_hd_chain_to_inv_list: hd_chain is NULL");
    check(hd_chain -> len > 0, "Failed to kyk_hd_chain_to_inv_list: hd_chain -> len is invalid");

    inv_list = calloc(hd_chain -> len, sizeof(*inv));
    check(inv_list, "Failed to kyk_hd_chain_to_inv_list: calloc failed");    

    for(i = 0; i < hd_chain -> len; i++){
	hd = hd_chain -> hd_list + i;
	inv = inv_list + i;
	inv -> type = type;
	kyk_blk_hash256(digest, hd);
	memcpy(inv -> hash, digest, sizeof(digest));
    }

    *new_inv_list = inv_list;/* -----------+**------------------------------------------------------------------------------------------------------*+ cat's codes */
    *inv_count = hd_chain -> len;

    return 0;

error:

    return -1;
}


int kyk_seri_blk_to_new_pld(ptl_payload** new_pld, const struct kyk_block* blk)
{
    ptl_payload* pld = NULL;
    size_t checknum = 0;
    int res = -1;

    check(blk, "Failed to kyk_seri_blk_to_new_pld: blk is NULL");
    check(blk -> blk_size > 0, "Failed to kyk_seri_blk_to_new_pld: blk -> blk_size is invalid");
    check(blk -> hd, "Failed to kyk_seri_blk_to_new_pld: blk -> hd is NULL");
    check(blk -> tx, "Failed to kyk_seri_blk_to_new_pld: blk -> tx is NULL");

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_seri_blk_to_new_pld: calloc failed");

    pld -> len = blk -> blk_size;
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));

    res = kyk_seri_blk(pld -> data, blk, &checknum);
    check(res == 0, "Failed to kyk_seri_blk_to_new_pld: kyk_seri_blk failed");
    check(checknum == pld -> len, "Failed to kyk_seri_blk_to_new_pld: kyk_seri_blk failed");

    *new_pld = pld;

    return 0;
    
error:
    if(pld) kyk_free_ptl_payload(pld);
    return -1;
}

int kyk_build_new_reject_ptl_payload(ptl_payload** new_pld,
				     const var_str* message,
				     uint8_t ccode,
				     const var_str* reason,
				     uint8_t* data,
				     size_t data_len)
{
    ptl_payload* pld = NULL;
    uint8_t* bufp = NULL;

    check(new_pld, "Failed to kyk_build_new_reject_ptl_payload: new_pld is NULL");
    check(message, "Failed to kyk_build_new_reject_ptl_payload: message is NULL");
    check(reason, "Failed to kyk_build_new_reject_ptl-payload: reason is NULL");
    
    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_build_new_reject_ptl_payload: calloc failed");

    pld -> len = get_var_str_size(message) + sizeof(ccode) + get_var_str_size(reason) + data_len;
    pld -> data = calloc(pld -> len, sizeof(*pld -> data));
    check(pld -> data, "Failed to kyk_build_new_reject_ptl_payload: calloc failed");

    bufp = pld -> data;

    bufp += kyk_pack_var_str(bufp, message);
    
    *bufp = ccode;
    bufp += 1;

    bufp += kyk_pack_var_str(bufp, reason);

    if(data){
	check(data_len == 32, "Failed to kyk_build_new_reject_ptl_payload: data len is invalid");
	memcpy(bufp, data, data_len);
    }
    

    *new_pld = pld;

    return 0;

error:
    if(pld) kyk_free_ptl_payload(pld);
    return -1;
}

int kyk_deseri_new_reject_entity(const uint8_t* buf,
				 size_t buf_len,
				 ptl_reject_entity** new_entity,
				 size_t* checknum)
{
    ptl_reject_entity* et = NULL;
    const uint8_t* bufp = NULL;
    size_t len = 0;
    int res = -1;
    size_t data_len = 0;
    
    check(buf, "Failed to kyk_deseri_new_reject_entity: buf is NULL");

    et = calloc(1, sizeof(*et));
    check(et, "Failed to kyk_deseri_new_reject_entity: calloc failed");

    bufp = buf;

    res = kyk_unpack_var_str(bufp, &et -> message, &len);
    check(res == 0, "Failed to kyk_deseri_new_reject_entity: kyk_unpack_var_str failed");
    bufp += len;

    et -> ccode = *bufp;
    bufp += sizeof(et -> ccode);

    res = kyk_unpack_var_str(bufp, &et -> reason, &len);
    check(res == 0, "Failed to kyk_deseri_new_reject_entity: kyk_unpack_var_str failed");
    bufp += len;

    if(bufp - buf < (long)buf_len){
	data_len = buf_len - (bufp - buf);
	check(data_len == 32, "Failed to kyk_deseri_new_reject_entity: invalid data len");
	et -> data = calloc(data_len, sizeof(*et -> data));
	check(et -> data, "Failed to kyk_deseri_new_reject_entity: calloc failed");
	memcpy(et -> data, bufp, data_len);
	bufp += data_len;
    }

    *new_entity = et;

    if(checknum){
	*checknum = bufp - buf;
    }
    
    return 0;

error:
    if(et) kyk_free_ptl_reject_entity(et);
    return -1;
}

void kyk_print_ptl_reject_entity(const ptl_reject_entity* et)
{
    printf("reject_entity -> message: %s\n", et -> message -> data);
    switch(et -> ccode){
    case CC_REJECT_MALFORMED:
	printf("reject_entity -> ccode: CC_REJECT_MALFORMED\n");
	break;
    case CC_REJECT_INVALID:
	printf("reject_entity -> ccode: CC_REJECT_INVALID\n");
	break;
    case CC_REJECT_OBSOLETE:
	printf("reject_entity -> ccode: CC_REJECT_OBSOLETE\n");
	break;
    case CC_REJECT_DUPLICATE:
	printf("reject_entity -> ccode: CC_REJECT_DUPLICATE\n");
	break;
    case CC_REJECT_NONSTANDARD:
	printf("reject_entity -> ccode: CC_REJECT_NONSTANDARD\n");
	break;
    case CC_REJECT_DUST:
	printf("reject_entity -> ccode: CC_REJECT_DUST\n");
	break;
    case CC_REJECT_INSUFFICIENTFEE:
	printf("reject_entity -> ccode: CC_REJECT_INSUFFICIENTFEE\n");
	break;
    case CC_REJECT_CHECKPOINT:
	printf("reject_entity -> ccode: CC_REJECT_CHECKPOINT\n");
	break;
    default:
	printf("reject_entity -> ccode: %d\n", et -> ccode);
	break;
    }
    printf("reject_entity -> reason: %s\n", et -> reason -> data);
    if(et -> data){
	kyk_print_hex("reject_entity -> data", et -> data, 32);
    }
}

void kyk_free_ptl_reject_entity(ptl_reject_entity* et)
{
    if(et){
	if(et -> message){
	    kyk_free_var_str(et -> message);
	    et -> message = NULL;
	}

	if(et -> reason){
	    kyk_free_var_str(et -> reason);
	    et -> reason = NULL;
	}

	if(et -> data){
	    free(et -> data);
	    et -> data = NULL;
	}

	free(et);
    }
}

int kyk_seri_tx_to_new_pld(ptl_payload** new_pld, const struct kyk_tx* tx)
{
    ptl_payload* pld = NULL;
    int res = -1;

    check(tx, "Failed to kyk_seri_tx_to_new_pld: tx is NULL");

    pld = calloc(1, sizeof(*pld));
    check(pld, "Failed to kyk_seri_tx_to_new_pld: calloc failed");

    res = kyk_seri_tx_to_new_buf(tx, &pld -> data, (size_t*)&pld -> len);
    check(res == 0, "Failed to kyk_seri_tx_to_new_pld: kyk_seri_tx_to_new_buf failed");

    *new_pld = pld;

    return 0;

error:
    if(pld) kyk_free_ptl_payload(pld);
    return -1;
}

