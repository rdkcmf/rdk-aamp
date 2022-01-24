#!/bin/bash

function package_exists_lin() {
    dpkg -s "$1" &> /dev/null
    return $?
}

function install_package() {
    if ! package_exists_lin $1 ; then
        echo "installing $1"
        apt install $1 -y
    fi
}

install_package git
install_package cmake
install_package gcc
install_package g++
install_package libcurl4-openssl-dev
install_package libgstreamer1.0-dev 
install_package libgstreamer-plugins-bad1.0-dev 
install_package libssl-dev 
install_package libxml2-dev 
install_package pkg-config 
install_package zlib1g-dev