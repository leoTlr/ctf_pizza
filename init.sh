#!/bin/bash

# test if neccessary files exist and build exec
#
# -f or --new will delete and recreate db and files in build dir
#
# generating new keys can result in scorebot failing to recieve flag again
# if you really want to do this, just delete the keys and run this script again

DIR=$(pwd)
NEW=0

if [[ $1 == '-n' || $1 == '--new' ]]; then
    NEW=1
fi

# adjust paths here if they dont fit

DB_FILE="$DIR/order.db"
DB_INITFILE="$DIR/dbinit.sql"

KEY_DIR="$DIR/rsa_keys"
KPUB_NAME="pub_key.pem"
KPRIV_NAME="priv_key.pem"
KPUB_FILE="$KEY_DIR/$KPUB_NAME"
KPRIV_FILE="$KEY_DIR/$KPRIV_NAME"

BDIR="$DIR/build"

#############################################################################################

function generate_keys {

    echo -n "[*] generating new rsa keypair ... "
    openssl genrsa -out "$KPRIV_FILE"  1>/dev/null 2>/dev/null && 
    openssl rsa -pubout -in "$KPRIV_FILE" -outform PEM -out "$KPUB_FILE" 1>/dev/null 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "OK"
    else
        echo -e "failed"
        echo "[ERROR] need both a pub and priv rsa key file in .pem format"
        echo "[INFO] try following commands: "
        echo "openssl genrsa -out $KPRIV_FILE"
        echo "openssl rsa -pubout -in $KPRIV_FILE -outform PEM -out $KPUB_FILE"
        exit 1
    fi
}

function create_db {

    echo -n "[*] creating db with initfile ... "
    if [ ! -f $DB_INITFILE ]; then
        echo "failed"
        echo "[ERROR] $DB_INITFILE not found"
        exit 1
    fi
    sqlite3 $DB_FILE < $DB_INITFILE
    if [ $? -eq 0 ]; then
        echo "OK"
    else
        echo "[ERROR] db initialization failed"
        exit 1
    fi
}

function build_exec {

    echo "[*] building executable ... "
    cd $BDIR && cmake ../ && make -j4

    if [ $? -ne 0 ]; then
        echo "[ERROR] build failed :("
        exit 1
    fi
}

mkdir -p $KEY_DIR $BDIR 2>/dev/null

# generate rsa keypair in required format
echo -n "[*] check if rsa keys exists ... "
if [[ -d $KEY_DIR && -f $KPUB_FILE && -f $KPRIV_FILE && $NEW -eq 0 ]]; then
    echo "OK"
elif [ $NEW -ne 0 ]; then
    echo "--new used"
    echo "[*] removing old keys"
    rm -f $KPUB_FILE $KPRIV_FILE
    generate_keys
else
    echo "not found"
    generate_keys
fi

# create db if not exists
echo -n "[*] check if database exists ... "
if [[ -f $DB_FILE && $NEW -eq 0 ]]; then
    echo "OK"
elif [ $NEW -ne 0 ]; then
    echo "--new used"
    echo "[*] removing old db"
    rm -f $DB_FILE
    create_db
else
    echo "not found"
    create_db
fi


echo -n "[*] check if build dir exists ... "
if [[ -d $BDIR && $NEW -eq 0 ]]; then
    echo "OK"
elif [ $NEW -ne 0 ]; then
    echo "--new used"
    echo "[*] clearing files in build dir: $BDIR"
    rm -rf $BDIR/*
else
    echo "not found"
    echo "[*] creating build dir: $BDIR"
    mkdir $BDIR
fi

build_exec

echo "[INFO] keys at $KEY_DIR"
echo "[INFO] db at $DB_FILE"