#pragma once

namespace infinit
{
  namespace cli
  {
    class Block;
    class Credentials;
#if INFINIT_WITH_DAEMON
    class Daemon;
#endif
    class Device;
    class Doctor;
    class Drive;
    class Infinit;
    class Journal;
#if INFINIT_WITH_LDAP
    class LDAP;
#endif
    class Network;
    class Passport;
    class Silo;
    class User;
    class Volume;
  }
}
