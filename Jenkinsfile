#!/bin/env groovy
// This is a Jenkins pipeline file. It will be picked up by Jenkins and contains
// instructions this project should be built and tested.

pipeline {
    agent {
        node {
            label 'linux'
        }
    }

    stages {
        stage('Build') {
            steps {
                sh '''
                 autoreconf --install
                 ./configure --prefix=$PWD/image
                 make -j 8
                 '''
            }
        }

        stage('Test') {
            steps {
                sh 'make check'
            }
        }

        stage('Push github') {
              when { branch 'master' } // Only run this step on the master branch
              steps {
                sh 'git push git@github.com:topic-embedded-products/libdyplo.git HEAD:refs/heads/master'
            }
        }
    }
}
