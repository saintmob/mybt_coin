#include "kyk_wallet.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kyk_utils.h"
#include "gens_block.h"
#include "block_store.h"
#include "kyk_ldb.h"
#include "kyk_blk_file.h"
#include "dbg.h"
#include "kyk_ser.h"
#include "kyk_buff.h"

const static char* BLOCKS_DIR =  "blocks";
const static char*  IDX_DB_NAME = "index";

struct kyk_wallet {
    char *wdir;
    char *blk_dir;
    char *idx_db_path;
    struct kyk_block_db* blk_index_db;
};

static void set_init_bval(struct kyk_bkey_val *bval,
			  const struct kyk_block* blk,
			  const struct kyk_blk_file* blk_file
    );

static int load_init_data_to_wallet(struct kyk_wallet *wallet);
void kyk_set_fdir(const char* fdir);
struct kyk_wallet* new_wallet(const char *wdir);
int kyk_save_blk_to_file(struct kyk_blk_file* blk_file,
			 const struct kyk_block* blk
    );


struct kyk_wallet* kyk_init_wallet(const char *wdir)
{
    int res = 0;
    struct kyk_wallet* wallet = new_wallet(wdir);
    check(wallet != NULL, "failed to get a new wallet");

    kyk_set_fdir(wallet -> blk_dir);
    kyk_init_store_db(wallet -> blk_index_db, wallet -> idx_db_path);
    check(wallet -> blk_index_db -> errptr == NULL, "failed to init block index db");
    
    res = load_init_data_to_wallet(wallet);
    check(res > 0, "failed to init wallet");
    
    return wallet;
    
error:
    if(wallet) kyk_destroy_wallet(wallet);
    return NULL;
}

struct kyk_wallet* new_wallet(const char *wdir)
{
    struct kyk_wallet* wallet = malloc(sizeof(struct kyk_wallet));
    check(wallet != NULL, "failed to malloc wallet");
    
    wallet -> blk_index_db = malloc(sizeof(struct kyk_block_db));
    check(wallet -> blk_index_db != NULL, "failed to malloc block index db");
    
    wallet -> wdir = malloc(strlen(wdir) + 1);
    check(wallet -> wdir != NULL, "failed to malloc wdir");
    strncpy(wallet -> wdir, wdir, strlen(wdir) + 1);

    wallet -> blk_dir = kyk_pth_concat(wallet -> wdir, BLOCKS_DIR);
    check(wallet -> blk_dir != NULL, "failed to get block dir");
    
    wallet -> idx_db_path = kyk_pth_concat(wallet -> blk_dir, IDX_DB_NAME);
    check(wallet -> idx_db_path != NULL, "failed to get idx db path");

    return wallet;

error:
    if(wallet) kyk_destroy_wallet(wallet);
    return NULL;
}

void kyk_set_fdir(const char* fdir)
{
    if(kyk_detect_dir(fdir) != 1){
	mkdir(fdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
}

struct kyk_wallet* kyk_open_wallet(const char *wdir)
{
    struct kyk_wallet* wallet = new_wallet(wdir);
    check(wallet != NULL, "failed to get a new wallet");

    kyk_init_store_db(wallet -> blk_index_db, wallet -> idx_db_path);
    check(wallet -> blk_index_db -> errptr == NULL, "failed to open block index db");
    
    return wallet;
    
error:
    if(wallet) kyk_destroy_wallet(wallet);    
    return NULL;
}

struct kyk_bkey_val* w_get_block(const struct kyk_wallet* wallet, const char* blk_hash_str, char **errptr)
{
    //struct kyk_block* blk;
    struct kyk_bkey_val* bval = NULL;
    char blk_hash[32];
    size_t len = strlen(blk_hash_str);
    check(len == 64, "invalid block hash");

    kyk_parse_hex((uint8_t*)blk_hash, blk_hash_str);
    bval = kyk_read_block(wallet -> blk_index_db, blk_hash, errptr);

    return bval;
error:
    if(bval) kyk_free_bval(bval);
    return NULL;
}

void kyk_destroy_wallet(struct kyk_wallet* wallet)
{
    if(wallet -> wdir) free(wallet -> wdir);
    if(wallet -> blk_dir) free(wallet -> blk_dir);
    if(wallet -> idx_db_path) free(wallet -> idx_db_path);
    if(wallet -> blk_index_db) kyk_free_block_db(wallet -> blk_index_db);
}

int load_init_data_to_wallet(struct kyk_wallet *wallet)
{
    struct kyk_block *blk = NULL;
    struct kyk_bkey_val bval;
    struct kyk_blk_file* blk_file = NULL;
    int res = 1;
    char *errptr = NULL;
    
    blk = make_gens_block();
    check(blk != NULL, "failed to make gens block");

    blk_file = kyk_create_blk_file(0, wallet -> blk_dir, "ab");
    check(blk_file != NULL, "failed to create block file");

    res = kyk_save_blk_to_file(blk_file, blk);
    check(res == 1, "failed to save block to file");
    
    set_init_bval(&bval, blk, blk_file);
    kyk_store_block(wallet -> blk_index_db, &bval, &errptr);
    check(errptr == NULL, "failed to store b key value");

    kyk_free_block(blk);
    kyk_close_blk_file(blk_file);
    
    return res;

error:
    if(blk) kyk_free_block(blk);
    if(blk_file) kyk_close_blk_file(blk_file);
    return -1;
}

void set_init_bval(struct kyk_bkey_val *bval,
		   const struct kyk_block* blk,
		   const struct kyk_blk_file* blk_file
    )
{
    bval -> wVersion = 1;
    bval -> nHeight = 0;
    bval -> nStatus = BLOCK_HAVE_MASK;
    bval -> nTx = blk -> tx_count;
    bval -> nFile = blk_file -> nFile;
    bval -> nDataPos = blk_file -> nStartPos;
    bval -> nUndoPos = 0;
    bval -> blk_hd = blk -> hd;
}

int kyk_save_blk_to_file(struct kyk_blk_file* blk_file,
			   const struct kyk_block* blk
    )
{
    struct kyk_buff* buf = NULL;
    size_t len = 0;
    long int pos = 0;

    buf = create_kyk_buff(1000);
    check(buf != NULL, "failed to create kyk buff");
    
    len = kyk_ser_blk_for_file(buf, blk);
    check(len > 0, "failed to serialize block");
    
    pos = ftell(blk_file -> fp);
    check(pos != -1L, "failed to get the block dat file pos");

    blk_file -> nOffsetPos = sizeof(blk -> magic_no) + sizeof(blk -> blk_size);
    
    blk_file -> nStartPos = (unsigned int)pos + blk_file -> nOffsetPos;
    
    len = fwrite(buf -> base, sizeof(uint8_t), buf -> len, blk_file -> fp);
    check(len == buf -> len, "failed to save block to file");
    blk_file -> nEndPos = len;
    

    free_kyk_buff(buf);
    return 1;
error:
    if(buf) free_kyk_buff(buf);
    return -1;
}

