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
install_package libreadline-dev
install_package libgstreamer-plugins-base1.0-dev
install_package gstreamer1.0-libav

ver=$(grep -oP 'VERSION_ID="\K[\d.]+' /etc/os-release)

if [ ${ver:0:2} -eq 20 ]; then
	echo "No additional packages required. OS verion is $ver"
elif [ ${ver:0:2} -eq 18 ]; then
	echo "Two additional packages required. 0S version is $ver"
	install_package freeglut3-dev
	install_package libglew-dev
else
	echo "Please upgrade your Ubuntu version to at least 18:04 LTS. OS version is $ver"
fi
