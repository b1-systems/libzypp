## Configuration file for software management
## /etc/zypp/zypp.conf
##
## Boolean values are 0 1 yes no on off true false


[main]


##
## Override the detected architecture
##
## Valid values:  i586, i686, x86_64, ppc, ppc64, ia64, s390, s390x, ..
## Default value: Autodetected
##
## ** CAUTION: Only set if you know what you're doing !
## ** Changing this needs a full refresh (incl. download)
## ** of all repository data.
##
# arch = s390


##
## Path where the repo metadata is downloaded and kept.
##
## Valid values: A directory
## Default value: /var/cache/zypp/raw
##
## Changing this needs a full refresh (incl. download) of all repository data
##
# metadatadir = /var/cache/zypp/raw


##
## Path where the known repositories .repo files are kept
##
## Valid values: A directory
## Default value: /etc/zypp/repos.d
##
## Changing this invalidates all known repositories
##
# reposdir = /etc/zypp/repos.d


##
## Path where the processed cache is kept (this is where zypp.db is located)
##
## Valid values: A directory
## Default value: /var/cache/zypp
##
## Changing this needs a full refresh (except download) of all repository data
##
# cachedir = /var/cache/zypp


##
## Whether repository urls should be probed when added
##
## Valid values: boolean
## Default value: false
##
## If true, accessability of repositories is checked immediately (when added)
##   (e.g. 'zypper ar' will check immediately)
## If false, accessability of repositories is checked when refreshed
##   (e.g. 'zypper ar' will delay the check until the next refresh)
##
# repo.add.probe = false


##
## Amount of time in minutes that must pass before another refresh.
##
## Valid values: Integer
## Default value: 10
##
## If you have autorefresh enabled for a repository, it is checked for
## up-to-date metadata not more often than every <repo.refresh.delay>
## minutes. If an automatic request for refresh comes before <repo.refresh.delay>
## minutes passed since the last check, the request is ignored.
##
## A value of 0 means the repository will always be checked. To get the oposite
## effect, disable autorefresh for your repositories.
##
## This option has no effect for repositories with autorefresh disabled, nor for
## user-requested refresh.
##
# repo.refresh.delay = 10


##
## Whether to consider using a .patch.rpm when downloading a package
##
## Valid values: boolean
## Default value: true
##
## Using a patch rpm will decrease the download size for package updates
## since it does not contain all files of the package but only the changed
## ones. The .patch.rpm is ready to be installed immediately after download.
## There is no further processing needed, as it is for a .delta.rpm.
##
# download.use_patchrpm = true


##
## Whether to consider using a .delta.rpm when downloading a package
##
## Valid values: boolean
## Default value: true
##
## Using a delta rpm will decrease the download size for package updates
## since it does not contain all files of the package but only the binary
## diff of changed ones. Recreating the rpm package on the local machine
## is an expensive operation (memory,CPU). If your network connection is
## not too slow, you benefit from disabling .delta.rpm.
##
# download.use_deltarpm = true


## 
## Defining directory for equivalent vendors
##
## Valid values: A directory
## Default value: /etc/zypp/vondors.d
##
#vendordir = ./../../tests/zypp/data/Vendor/vendors.d
vendordir = @VENDOR_D@

