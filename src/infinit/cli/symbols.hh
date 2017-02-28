#pragma once

#include <elle/das/Symbol.hh>

#include <infinit/utility.hh>

#ifdef INFINIT_WINDOWS
# undef stat
#endif

namespace infinit
{
  namespace cli
  {
    // Symbol, short option char (or 0), help string, whether positional argument.
    ELLE_DAS_CLI_SYMBOL(account, '\0', "cloud service account name", false);
    ELLE_DAS_CLI_SYMBOL(add, '\0', "add users, administrators and groups to group (prefix: @<group>, ^<admin>", false);
    ELLE_DAS_CLI_SYMBOL(add_admin, '\0', "add administrator to group", false);
    ELLE_DAS_CLI_SYMBOL(add_group, '\0', "add group to group", false);
    ELLE_DAS_CLI_SYMBOL(add_user, '\0', "add user to group", false);
    ELLE_DAS_CLI_SYMBOL(admin_r, 0, "Set admin users that can read all data", false);
    ELLE_DAS_CLI_SYMBOL(admin_remove, 0, "Remove given users from all admin lists (prefix: @<group>, requires mountpoint)", false);
    ELLE_DAS_CLI_SYMBOL(admin_rw, 0, "Set admin users that can read and write all data", false);
    ELLE_DAS_CLI_SYMBOL(advertise_host, 0, "advertise extra endpoint using given host", false);
    ELLE_DAS_CLI_SYMBOL(all, 0, "all information", false);
    ELLE_DAS_CLI_SYMBOL(allow_create_passport, 0, "allow user to create passports for network", false);
    ELLE_DAS_CLI_SYMBOL(allow_root_creation, 0, "create the filesystem root if not found" , false);
    ELLE_DAS_CLI_SYMBOL(as, 'a', "user to run commands as", false);
    ELLE_DAS_CLI_SYMBOL(async, 0, "use asynchronous write operations" , false);
    ELLE_DAS_CLI_SYMBOL(avatar, '\0', "path to an image to use as avatar", false);
    ELLE_DAS_CLI_SYMBOL(aws, 0, "Amazon Web Services (or S3 compatible) credentials", false);
    ELLE_DAS_CLI_SYMBOL(block_size, '\0', "{object} block size", false);
    ELLE_DAS_CLI_SYMBOL(bucket, '\0', "bucket name", false);
    ELLE_DAS_CLI_SYMBOL(cache, 0, "enable caching with default values", false);
    ELLE_DAS_CLI_SYMBOL(cache_disk_size, 0, "size of disk cache for immutable data in bytes (default: 512MB)", false);
    ELLE_DAS_CLI_SYMBOL(cache_ram_invalidation, 0, "RAM block cache invalidation time in seconds (default: 15s)", false);
    ELLE_DAS_CLI_SYMBOL(cache_ram_size, 0, "maximum RAM block cache size in bytes (default: 64MB)", false);
    ELLE_DAS_CLI_SYMBOL(cache_ram_ttl, 0, "RAM block cache time-to-live in seconds (default: 5min)", false);
    ELLE_DAS_CLI_SYMBOL(capacity, 'c', "limit storage capacity (use: B,kB,kiB,MB,MiB,GB,GiB,TB,TiB)", false);
    ELLE_DAS_CLI_SYMBOL(clear_content, '\0', "remove all blocks from disk (filesystem storage only)", false);
    ELLE_DAS_CLI_SYMBOL(compatibility_version, '\0', "compatibility version to force", false);
    ELLE_DAS_CLI_SYMBOL(create, 'c', "create the {object}", false);
    ELLE_DAS_CLI_SYMBOL(create_home, 0, "create user home directory of the form home/<user>", false);
    ELLE_DAS_CLI_SYMBOL(create_root, 'R', "create root directory", false);
    ELLE_DAS_CLI_SYMBOL(daemon, 'd', "run as a background daemon" , false);
    ELLE_DAS_CLI_SYMBOL(default_network, 0, "Default network for volume creation", false);
    ELLE_DAS_CLI_SYMBOL(default_permissions, 'd', "default permissions (optional: r,rw)", false);
    ELLE_DAS_CLI_SYMBOL(deny_storage, '\0', "deny user ability to contribute storage to the network", false);
    ELLE_DAS_CLI_SYMBOL(deny_write, '\0', "deny user write access to the network", false);
    ELLE_DAS_CLI_SYMBOL(description, '\0', "{object} description", false);
    ELLE_DAS_CLI_SYMBOL(disable_UTF_8_conversion, 0, "disable FUSE conversion of UTF-8 to native format", false);
    ELLE_DAS_CLI_SYMBOL(disable_inherit, '\0', "make new files and directories not inherit permissions", false);
    ELLE_DAS_CLI_SYMBOL(docker, 0, "Enable the Docker plugin", false);
    ELLE_DAS_CLI_SYMBOL(docker_descriptor_path, 0, "Path to add plugin descriptor", false);
    ELLE_DAS_CLI_SYMBOL(docker_home, 0, "Home directory to use for Docker user (default: /home/<docker-user>)", false);
    ELLE_DAS_CLI_SYMBOL(docker_mount_substitute, 0, "[from:to|prefix] : Substitute 'from' to 'to' in advertised path", false);
    ELLE_DAS_CLI_SYMBOL(docker_socket_path, 0, "Path for plugin socket", false);
    ELLE_DAS_CLI_SYMBOL(docker_socket_port, 0, "TCP port to use to communicate with Docker, 0 for random", false);
    ELLE_DAS_CLI_SYMBOL(docker_socket_tcp, 0, "Use a TCP socket for docker plugin", false);
    ELLE_DAS_CLI_SYMBOL(docker_user, 0, "System user to use for docker plugin",false);
    ELLE_DAS_CLI_SYMBOL(domain, 'd', "LDAP domain", false);
    ELLE_DAS_CLI_SYMBOL(email, 'e', "user email", false);
    ELLE_DAS_CLI_SYMBOL(email_pattern, 'e', "email address pattern)" , false);
    ELLE_DAS_CLI_SYMBOL(enable_inherit, 'i', "make new files and directories inherit permissions", false);
    ELLE_DAS_CLI_SYMBOL(encrypt, 0,  "use encryption: no, lazy, yes (default: yes)", false);
    ELLE_DAS_CLI_SYMBOL(endpoint, '\0', "S3 endpoint", false);
    ELLE_DAS_CLI_SYMBOL(endpoints_file, 0, "write node listening endpoints to file (format: host:port)", false);
    ELLE_DAS_CLI_SYMBOL(eviction_delay, 'e', "missing servers eviction delay (default: 10min)", false);
    ELLE_DAS_CLI_SYMBOL(fallback_xattrs, '\0', "use fallback special file if extended attributes are not supported", false);
    ELLE_DAS_CLI_SYMBOL(fetch, 'f', "fetch {object} from {hub}", false);
    ELLE_DAS_CLI_SYMBOL(fetch_drive, 0, "update local drive descriptor from {hub}", false);
    ELLE_DAS_CLI_SYMBOL(fetch_endpoints, 0, "fetch endpoints from {hub}" , false);
    ELLE_DAS_CLI_SYMBOL(fetch_endpoints_interval, 0, "period for repolling endpoints from the Hub in seconds", false);
    ELLE_DAS_CLI_SYMBOL(filter, 'f', "raw LDAP query to use (default: objectClass=posixGroup)", false);
    ELLE_DAS_CLI_SYMBOL(finder_sidebar, 0, "show volume in Finder sidebar" , false);
    ELLE_DAS_CLI_SYMBOL(force, 'f', "do not ask for user confirmation", false);
    ELLE_DAS_CLI_SYMBOL(full, '\0', "include private key (do not use unless you understand the implications", false);
    ELLE_DAS_CLI_SYMBOL(fullname, '\0', "user full name", false);
    ELLE_DAS_CLI_SYMBOL(fullname_pattern, 'F', "fullname pattern)" , false);
    ELLE_DAS_CLI_SYMBOL(fuse_option, 0, "option to pass directly to FUSE" , false);
    ELLE_DAS_CLI_SYMBOL(group, 'g', "group {action} {object} for", false);
    ELLE_DAS_CLI_SYMBOL(grpc, 0, "start grpc server on given endpoint", false);
    ELLE_DAS_CLI_SYMBOL(help, 'h', "show this help message", false);
    ELLE_DAS_CLI_SYMBOL(hold, 0, "keep storage online until this process terminates", false);
    ELLE_DAS_CLI_SYMBOL(home, 'h', "create a home directory for the invited user", false);
    ELLE_DAS_CLI_SYMBOL(host, '\0', "SSH host", false);
    ELLE_DAS_CLI_SYMBOL(icon, 'i', "path to an image to use as icon", false);
    ELLE_DAS_CLI_SYMBOL(ignore_non_linked, 0, "do not consider problematic non-linked networks", false);
    ELLE_DAS_CLI_SYMBOL(input, 'i', "file to read {object} from", false);
    ELLE_DAS_CLI_SYMBOL(k, 0, "number of groups (default: 1)", false);
    ELLE_DAS_CLI_SYMBOL(kalimero, 0, "use a Kalimero overlay network. Used for local testing", false);
    ELLE_DAS_CLI_SYMBOL(kelips, 0, "use a Kelips overlay network (default)", false);
    ELLE_DAS_CLI_SYMBOL(kelips_contact_timeout, 0, "ping timeout before considering a peer lost (default: 2min)", false);
    ELLE_DAS_CLI_SYMBOL(key, 'k', "RSA key pair in PEM format - e.g. your SSH key", false);
    ELLE_DAS_CLI_SYMBOL(kouncil, 0, "use a Kouncil overlay network", false);
    ELLE_DAS_CLI_SYMBOL(ldap_name, 'l', "user LDAP distinguished name", false);
    ELLE_DAS_CLI_SYMBOL(listen, 0, "specify which IP address to listen on (default: all)", false);
    ELLE_DAS_CLI_SYMBOL(log_level, 0, "Log level to start volumes with (default: LOG)", false);
    ELLE_DAS_CLI_SYMBOL(log_path, 0, "Store volume logs in given path", false);
    ELLE_DAS_CLI_SYMBOL(login_user, 0, "Login with selected user(s), of form 'user:password'", false);
    ELLE_DAS_CLI_SYMBOL(map_other_permissions, 0, "allow chmod to set world permissions", false);
    ELLE_DAS_CLI_SYMBOL(mode, 'm', "access mode {action}: r, w, rw, none", false);
    ELLE_DAS_CLI_SYMBOL(monitoring, 0, "enable monitoring", false);
    ELLE_DAS_CLI_SYMBOL(mount, 0, "mount given volumes on startup, keep trying on error", false);
    ELLE_DAS_CLI_SYMBOL(mount_icon, 0, "path to an icon for mounted volume" , false);
    ELLE_DAS_CLI_SYMBOL(mount_name, 0, "name of mounted volume" , false);
    ELLE_DAS_CLI_SYMBOL(mount_root, 0, "Default root path for all mounts", false);
    ELLE_DAS_CLI_SYMBOL(mountpoint, 'm', "where to mount the filesystem" , false);
    ELLE_DAS_CLI_SYMBOL(name, 'n', "name of the {object} {action}", true);
    ELLE_DAS_CLI_SYMBOL(network, 'N', "network {action} {object} for", false);
    ELLE_DAS_CLI_SYMBOL(no_avatar, '\0', "do not {action} avatars", false);
    ELLE_DAS_CLI_SYMBOL(no_color, 0, "don't use colored output", false);
    ELLE_DAS_CLI_SYMBOL(no_consensus, 0, "use no consensus algorithm", false);
    ELLE_DAS_CLI_SYMBOL(no_countdown, 0, "do not show countdown timer", false);
    ELLE_DAS_CLI_SYMBOL(no_local_endpoints, 0, "Disable automatic detection of local endpoints", false);
    ELLE_DAS_CLI_SYMBOL(no_public_endpoints, 0, "Disable automatic detection of public endpoints", false);
    ELLE_DAS_CLI_SYMBOL(node_id, 0, "node ID", false);
    ELLE_DAS_CLI_SYMBOL(nodes, 0, "estimate of the total number of nodes", false);
    ELLE_DAS_CLI_SYMBOL(object_class, 'o', "filter results (default: posixGroup)", false);
    ELLE_DAS_CLI_SYMBOL(operation, 'O', "operation to {action}", false);
    ELLE_DAS_CLI_SYMBOL(others_mode, 'o', "access mode {action} for other users: r, w, rw, none", false);
    ELLE_DAS_CLI_SYMBOL(output, 'o', "file to write the {object} to", false);
    ELLE_DAS_CLI_SYMBOL(packet_size, 's', "size of the packet to send (client only)", false);
    ELLE_DAS_CLI_SYMBOL(packets_count, 'n', "number of packets to exchange (client only)", false);
    ELLE_DAS_CLI_SYMBOL(passphrase, 0, "passphrase to secure identity (default: prompt for passphrase)", false);
    ELLE_DAS_CLI_SYMBOL(passport, 0, "create passports for each invitee", false);
    ELLE_DAS_CLI_SYMBOL(password, 'P', "password to authenticate with {hub}", false);
    ELLE_DAS_CLI_SYMBOL(path, '\0', "file whose {object} {action}", false);
    ELLE_DAS_CLI_SYMBOL(paths, 'p', "paths to blocks", false);
    ELLE_DAS_CLI_SYMBOL(paxos, 0, "use Paxos consensus algorithm (default)", false);
    ELLE_DAS_CLI_SYMBOL(paxos_rebalancing_auto_expand, 0, "whether to automatically rebalance under-replicated blocks", false);
    ELLE_DAS_CLI_SYMBOL(paxos_rebalancing_inspect, 0, "whether to inspect all blocks on startup and trigger rebalancing", false);
    ELLE_DAS_CLI_SYMBOL(peer, 0, "peer address or file with list of peer addresses (host:port)" , false);
    ELLE_DAS_CLI_SYMBOL(peers, 0, "list connected peers", false);
    ELLE_DAS_CLI_SYMBOL(peers_file, 0, "Periodically write list of known peers to given file", false);
    ELLE_DAS_CLI_SYMBOL(permissions, 0, "set default user permissions to XXX", false);
    ELLE_DAS_CLI_SYMBOL(port, 0, "outbound port to use", false);
    ELLE_DAS_CLI_SYMBOL(port_file, 0, "write node listening port to file", false);
    ELLE_DAS_CLI_SYMBOL(protocol, 0, "RPC protocol to use: tcp, utp, all (default: all)", false);
    ELLE_DAS_CLI_SYMBOL(publish, 0, "alias for --fetch-endpoints --push-endpoints" , false);
    ELLE_DAS_CLI_SYMBOL(pull, '\0', "pull {object} from {hub}", false);
    ELLE_DAS_CLI_SYMBOL(purge, '\0', "purge objects owned by the {object}", false);
    ELLE_DAS_CLI_SYMBOL(push, 'p', "push {object} to {hub}", false);
    ELLE_DAS_CLI_SYMBOL(push_drive, '\0', "push drive to {hub}", false);
    ELLE_DAS_CLI_SYMBOL(push_endpoints, 0, "push endpoints to {hub}" , false);
    ELLE_DAS_CLI_SYMBOL(push_invitations, '\0', "update remote drive descriptor and send invitations to {hub}", false);
    ELLE_DAS_CLI_SYMBOL(push_network, 0, "push the network to {hub}", false);
    ELLE_DAS_CLI_SYMBOL(push_passport, 0, "push passport to {hub}", false);
    ELLE_DAS_CLI_SYMBOL(push_user, 0, "push user to {hub}", false);
    ELLE_DAS_CLI_SYMBOL(push_volume, 0, "push the volume to {hub}" , false);
    ELLE_DAS_CLI_SYMBOL(readonly, 0, "mount as readonly" , false);
    ELLE_DAS_CLI_SYMBOL(receive, 0, "receive an object from another device using {hub}", false);
    ELLE_DAS_CLI_SYMBOL(recursive, 'R', "{verb} {object} recursively", false);
    ELLE_DAS_CLI_SYMBOL(redundancy, 0, "describe data redundancy", false);
    ELLE_DAS_CLI_SYMBOL(region, '\0', "AWS region", false);
    ELLE_DAS_CLI_SYMBOL(register_service, 'r', "register volume in the network", false);
    ELLE_DAS_CLI_SYMBOL(remove, '\0', "remove users, administrators and groups from group (prefix: @<group>, ^<admin>", false);
    ELLE_DAS_CLI_SYMBOL(remove_admin, '\0', "remove administrator from group", false);
    ELLE_DAS_CLI_SYMBOL(remove_group, '\0', "remove group from group", false);
    ELLE_DAS_CLI_SYMBOL(remove_user, '\0', "remove user from group", false);
    ELLE_DAS_CLI_SYMBOL(replication_factor, 'r', "data replication factor (default: 1)", false);
    ELLE_DAS_CLI_SYMBOL(restart, 0, "restart {object}", false);
    ELLE_DAS_CLI_SYMBOL(root_permissions, 0, "volume root permissions to give (optional: r, w, rw)", false);
    ELLE_DAS_CLI_SYMBOL(script, 's', "suppress extraneous human friendly messages and use JSON output", false);
    ELLE_DAS_CLI_SYMBOL(searchbase, 'b', "search starting point (without domain)", false); // FIXME: why not search_base?
    ELLE_DAS_CLI_SYMBOL(server, 0, "connectivity server address (default = 192.241.139.66)", false);
    ELLE_DAS_CLI_SYMBOL(service, 0, "fetch {object} from the network, not beyond", false);
    ELLE_DAS_CLI_SYMBOL(show, '\0', "list group users, administrators and description", false);
    ELLE_DAS_CLI_SYMBOL(ssh, '\0', "store blocks via SSH", false);
    ELLE_DAS_CLI_SYMBOL(stat, '\0', "show the remaining asynchronous operations count and size", false);
    ELLE_DAS_CLI_SYMBOL(storage, 'S', "storage to contribute (optional, data striped over multiple)", false);
    ELLE_DAS_CLI_SYMBOL(storage_class, '\0', "storage class to use: STANDARD, STANDARD_IA, REDUCED_REDUNDANCY (default: bucket default)", false);
    ELLE_DAS_CLI_SYMBOL(tcp_port, 't', "port to perform tcp tests on", false);
    ELLE_DAS_CLI_SYMBOL(traverse, 't', "set read permission on parent directories", false);
    ELLE_DAS_CLI_SYMBOL(upnp_tcp_port, 0, "port to try to get an tcp upnp connection on", false);
    ELLE_DAS_CLI_SYMBOL(upnp_udt_port, 0, "port to try to get an udt upnp connection on", false);
    ELLE_DAS_CLI_SYMBOL(user, 'u', "user {action} {object} for", false);
    ELLE_DAS_CLI_SYMBOL(username_pattern, 'U', "Hub unique username to set (default: $(cn)%). Remove the '%' to disable unique username generator", false);
    ELLE_DAS_CLI_SYMBOL(utp_port, 'u', "port to perform utp tests on (if unspecified, --xored_utp_port = utp_port + 1)", false);
    ELLE_DAS_CLI_SYMBOL(value, 'v', "value {action}", false);
    ELLE_DAS_CLI_SYMBOL(verbose, 'v', "use verbose output", false);
    ELLE_DAS_CLI_SYMBOL(volume, 'V', "associated volume name", false);
    ELLE_DAS_CLI_SYMBOL(xored, 'X', "performs test applying a 0xFF xor on the utp traffic, value=yes,no,both", false);
    ELLE_DAS_CLI_SYMBOL(xored_utp_port, 'x', "port to perform xored utp tests on", false);

    ELLE_DAS_CLI_SYMBOL_NAMED(delete, delete_, 0, "delete the {object}", false);

    ELLE_DAS_SYMBOL(acl);
    ELLE_DAS_SYMBOL(block);
    ELLE_DAS_SYMBOL(call);
    ELLE_DAS_SYMBOL(configuration);
    ELLE_DAS_SYMBOL(connectivity);
    ELLE_DAS_SYMBOL(credentials);
    ELLE_DAS_SYMBOL(describe);
    ELLE_DAS_SYMBOL(deserialize);
    ELLE_DAS_SYMBOL(device);
    ELLE_DAS_SYMBOL(disable_storage);
    ELLE_DAS_SYMBOL(doctor);
    ELLE_DAS_SYMBOL(drive);
    ELLE_DAS_SYMBOL(drive_invite);
    ELLE_DAS_SYMBOL(dropbox);
    ELLE_DAS_SYMBOL(enable_storage);
    ELLE_DAS_SYMBOL(filesystem);
    ELLE_DAS_SYMBOL(gcs);
    ELLE_DAS_SYMBOL(get_xattr);
    ELLE_DAS_SYMBOL(google_drive);
    ELLE_DAS_SYMBOL(hash);
    ELLE_DAS_SYMBOL(import);
    ELLE_DAS_SYMBOL(inspect);
    ELLE_DAS_SYMBOL(invite);
    ELLE_DAS_SYMBOL(join);
    ELLE_DAS_SYMBOL(journal);
    ELLE_DAS_SYMBOL(ldap);
    ELLE_DAS_SYMBOL(link);
    ELLE_DAS_SYMBOL(list);
    ELLE_DAS_SYMBOL(list_services);
    ELLE_DAS_SYMBOL(list_storage);
    ELLE_DAS_SYMBOL(login);
    ELLE_DAS_SYMBOL(manage_volumes);
    ELLE_DAS_SYMBOL(networking);
    ELLE_DAS_SYMBOL(populate_hub);
    ELLE_DAS_SYMBOL(populate_network);
    ELLE_DAS_SYMBOL(run);
    ELLE_DAS_SYMBOL(s3);
    ELLE_DAS_SYMBOL(set);
    ELLE_DAS_SYMBOL(set_xattr);
    ELLE_DAS_SYMBOL(signup);
    ELLE_DAS_SYMBOL(silo);
    ELLE_DAS_SYMBOL(start);
    ELLE_DAS_SYMBOL(stats);
    ELLE_DAS_SYMBOL(status);
    ELLE_DAS_SYMBOL(stop);
    ELLE_DAS_SYMBOL(system);
    ELLE_DAS_SYMBOL(transmit);
    ELLE_DAS_SYMBOL(unlink);
    ELLE_DAS_SYMBOL(update);
    ELLE_DAS_SYMBOL(version);

    ELLE_DAS_SYMBOL_NAMED(export, export_);
    ELLE_DAS_SYMBOL_NAMED(register, register_);

    // Modes must be defined twice: once as a regular symbol, then as
    // a `modes::mode_*` symbol.
    namespace modes
    {
      ELLE_DAS_SYMBOL(mode_add);
      ELLE_DAS_SYMBOL(mode_all);
      ELLE_DAS_SYMBOL(mode_configuration);
      ELLE_DAS_SYMBOL(mode_connectivity);
      ELLE_DAS_SYMBOL(mode_create);
      ELLE_DAS_SYMBOL(mode_delete);
      ELLE_DAS_SYMBOL(mode_describe);
      ELLE_DAS_SYMBOL(mode_deserialize);
      ELLE_DAS_SYMBOL(mode_disable_storage);
      ELLE_DAS_SYMBOL(mode_drive_invite);
      ELLE_DAS_SYMBOL(mode_dropbox);
      ELLE_DAS_SYMBOL(mode_enable_storage);
      ELLE_DAS_SYMBOL(mode_export);
      ELLE_DAS_SYMBOL(mode_fetch);
      ELLE_DAS_SYMBOL(mode_filesystem);
      ELLE_DAS_SYMBOL(mode_gcs);
      ELLE_DAS_SYMBOL(mode_get_xattr);
      ELLE_DAS_SYMBOL(mode_google_drive);
      ELLE_DAS_SYMBOL(mode_group);
      ELLE_DAS_SYMBOL(mode_hash);
      ELLE_DAS_SYMBOL(mode_import);
      ELLE_DAS_SYMBOL(mode_inspect);
      ELLE_DAS_SYMBOL(mode_invite);
      ELLE_DAS_SYMBOL(mode_join);
      ELLE_DAS_SYMBOL(mode_ldap);
      ELLE_DAS_SYMBOL(mode_link);
      ELLE_DAS_SYMBOL(mode_list);
      ELLE_DAS_SYMBOL(mode_list_services);
      ELLE_DAS_SYMBOL(mode_list_storage);
      ELLE_DAS_SYMBOL(mode_login);
      ELLE_DAS_SYMBOL(mode_manage_volumes);
      ELLE_DAS_SYMBOL(mode_mount);
      ELLE_DAS_SYMBOL(mode_networking);
      ELLE_DAS_SYMBOL(mode_populate_hub);
      ELLE_DAS_SYMBOL(mode_populate_network);
      ELLE_DAS_SYMBOL(mode_pull);
      ELLE_DAS_SYMBOL(mode_push);
      ELLE_DAS_SYMBOL(mode_receive);
      ELLE_DAS_SYMBOL(mode_register);
      ELLE_DAS_SYMBOL(mode_run);
      ELLE_DAS_SYMBOL(mode_s3);
      ELLE_DAS_SYMBOL(mode_set);
      ELLE_DAS_SYMBOL(mode_set_xattr);
      ELLE_DAS_SYMBOL(mode_signup);
      ELLE_DAS_SYMBOL(mode_start);
      ELLE_DAS_SYMBOL(mode_stat);
      ELLE_DAS_SYMBOL(mode_stats);
      ELLE_DAS_SYMBOL(mode_status);
      ELLE_DAS_SYMBOL(mode_stop);
      ELLE_DAS_SYMBOL(mode_system);
      ELLE_DAS_SYMBOL(mode_transmit);
      ELLE_DAS_SYMBOL(mode_unlink);
      ELLE_DAS_SYMBOL(mode_update);
    }
  }
}
