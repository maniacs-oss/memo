#! /bin/bash

set -e

rootdir=$1
nodes=$2
k=$3
replicas=3
observers=2
user=test
port_base=5050
nports=5 # allocate fixed port port_base+i to the first nports nodes

network_args="--kelips  --k $k --replication-factor $replicas"
#network_args="--kademlia"

# port(id, port_base, nports
function port {
  if test $1 -le $3; then echo "--port " $(($2 + $1)); fi
}

cd $rootdir

# Generate one main user $user, one user per node, and import $user in each
# also import all users in 0
mkdir store0
mkdir conf0
INFINIT_HOME=$rootdir/conf0 infinit-user --create --name $user
exported_user=$(INFINIT_HOME=$rootdir/conf0 infinit-user --export --name $user)
for i in $(seq 1 $nodes); do
  mkdir store$i
  mkdir conf$i
  INFINIT_HOME=$rootdir/conf$i infinit-user --create --name $user$i
  echo $exported_user | INFINIT_HOME=$rootdir/conf$i infinit-user --import
  INFINIT_HOME=$rootdir/conf$i infinit-user --export --name $user$i | INFINIT_HOME=$rootdir/conf0 infinit-user --import
done

# Generate storages
for i in $(seq 0 $nodes); do
  INFINIT_HOME=$rootdir/conf$i infinit-storage --create --capacity 2000000000 --name storage --filesystem --path $rootdir/store$i
done

# Generate overlay network and export structure
INFINIT_HOME=$rootdir/conf0 infinit-network --create --storage storage --name kelips --port $port_base --as $user $network_args
exported_network=$(INFINIT_HOME=$rootdir/conf0 infinit-network --export --as $user --name kelips)

# get hashed user name for $user
user_hash=$( cd conf0/networks && ls)
# import and join network
for i in $(seq 1 $nodes); do
  echo $exported_network | INFINIT_HOME=$rootdir/conf$i infinit-network --import
  INFINIT_HOME=$rootdir/conf0 infinit-passport --create --as $user --network kelips --user $user$i --output - \
  | INFINIT_HOME=$rootdir/conf$i infinit-passport --import
  INFINIT_HOME=$rootdir/conf$i infinit-network --link --as $user$i --name $user_hash/kelips --storage storage $(port $i $port_base $nports)
done

#observers
for i in $(seq 0 $observers); do
  mkdir observer_conf_$i
  mkdir observer_mount_$i
  INFINIT_HOME=$rootdir/observer_conf_$i infinit-user --create --name obs$i
  echo $exported_user | INFINIT_HOME=$rootdir/observer_conf_$i infinit-user --import
  INFINIT_HOME=$rootdir/observer_conf_$i infinit-user --export --name obs$i | INFINIT_HOME=$rootdir/conf0 infinit-user --import
  echo $exported_network | INFINIT_HOME=$rootdir/observer_conf_$i infinit-network --import

  INFINIT_HOME=$rootdir/conf0 infinit-passport --create --as $user --network kelips --user obs$i --output - \
  | INFINIT_HOME=$rootdir/observer_conf_$i infinit-passport --import
  INFINIT_HOME=$rootdir/observer_conf_$i infinit-network --link --as obs$i --name $user_hash/kelips
done

# create volume
INFINIT_HOME=$rootdir/conf0 infinit-volume --create --name kelips \
  --as $user --network kelips


exported_volume=$(INFINIT_HOME=$rootdir/conf0 infinit-volume --export --as $user --name kelips)
for i in $(seq 0 $observers); do
  echo $exported_volume | INFINIT_HOME=$rootdir/observer_conf_$i infinit-volume --import --mountpoint observer_mount_$i
done

echo '#!/bin/bash' > $rootdir/run-nodes.sh
chmod a+x $rootdir/run-nodes.sh
echo "INFINIT_HOME=$rootdir/conf0 infinit-volume --run --as $user --name $user_hash/kelips --mountpoint mount_0 --async --cache &" >> $rootdir/run-nodes.sh
echo 'sleep 2' >> $rootdir/run-nodes.sh
for i in $(seq 1 $nodes); do
  echo "INFINIT_HOME=$rootdir/conf$i infinit-network --run --as $user$i --name $user_hash/kelips --peer 127.0.0.1:$port_base &" >> $rootdir/run-nodes.sh
done

for i in $(seq 0 $observers); do
  echo "INFINIT_HOME=$rootdir/observer_conf_$i infinit-volume --run --mountpoint observer_mount_$i --as obs$i --name $user/kelips --peer 127.0.0.1:$port_base --cache --async" > $rootdir/run-volume-$i.sh
  chmod a+x $rootdir/run-volume-$i.sh
done
