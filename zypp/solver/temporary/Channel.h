/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* Channel.h
 * Copyright (C) 2000-2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 * Definition of 'edition'
 *  contains epoch-version-release-arch
 *  and comparision functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef ZYPP_SOLVER_TEMPORARY_CHANNEL_H
#define ZYPP_SOLVER_TEMPORARY_CHANNEL_H

#include <iosfwd>
#include <string>
#include <string>

#include "zypp/base/ReferenceCounted.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/base/PtrTypes.h"

#include "zypp/solver/temporary/ChannelPtr.h"
#include "zypp/solver/temporary/WorldPtr.h"
#include "zypp/solver/temporary/XmlNode.h"

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////

typedef std::list<Channel_Ptr> ChannelList;

typedef bool (*ChannelFn) (Channel_constPtr channel, void *data);
typedef bool (*ChannelAndSubscribedFn) (Channel_Ptr channel, bool flag, void *data);

typedef enum {

    CHANNEL_TYPE_ANY = 0,
    CHANNEL_TYPE_SYSTEM,
    CHANNEL_TYPE_NONSYSTEM,

    CHANNEL_TYPE_UNKNOWN,
    CHANNEL_TYPE_HELIX,
    CHANNEL_TYPE_DEBIAN,
    CHANNEL_TYPE_APTRPM,
    CHANNEL_TYPE_YAST,
    CHANNEL_TYPE_YUM

} ChannelType;

///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : Channel
/**
 *
 **/

class Channel : public base::ReferenceCounted, private base::NonCopyable {


  private:
    ChannelType _type;

    static int _fake_id;

    World_Ptr _world;

    std::string _id;
    std::string _legacy_id;			// Old ID for RCE servers

    std::string _name;
    std::string _alias;
    std::string _description;
					// priority if channel is...
    int _priority;			// subscribed
    int _priority_unsubscribed;		// unsubscribed

    std::string _path;
    std::string _file_path;
    std::string _icon_file;
    std::string _pkginfo_file;

//    std::list<std::string > *_distro_targets; /* List of targets (std::string ) for this channel */

    time_t _last_update;

    bool _system;
    bool _hidden;
    bool _immutable;

  public:

    Channel (ChannelType type) : _type (type), _world(NULL) {}

    Channel(const std::string & id = "", const std::string & name = "", const std::string & alias = "", const std::string & description = "");
    Channel(const XmlNode_Ptr node, int *subscribed, World_Ptr world);		//RCChannel *rc_channel_from_xml_node (xmlNode *channel_node);

    virtual ~Channel();

    // ---------------------------------- I/O

    const XmlNode_Ptr asXmlNode (void) const;		// rc_channel_to_xml_node

    static std::string toString ( const Channel & edition );

    virtual std::ostream & dumpOn( std::ostream & str ) const;

    friend std::ostream& operator<<( std::ostream&, const Channel& );

    std::string asString ( void ) const;

    // ---------------------------------- accessors

    ChannelType type(void) const { return _type; }
    void setType (ChannelType type) { _type = type; }

    std::string id (void) const { return _id; }
    void setId (const std::string & id) { _id = id; }

    World_Ptr world (void) const { return _world; }
    void setWorld (World_Ptr world) { _world = world; }

    std::string legacyId (void) const { return _legacy_id; }			// Old ID for RCE servers
    void setLegacyId (const std::string & legacy_id) { _legacy_id = legacy_id; }

    std::string name (void) const { return _name; }
    void setName (const std::string & name) { _name = name; }

    std::string alias (void) const { return _alias; }
    void setAlias (const std::string & alias) { _alias = alias; }

    std::string description (void) const { return _description; }
    void setDescription (const std::string & description) { _description = description; }

    int priority (void) const { return _priority; }
    void setPriority (int priority) { _priority = priority; }

    int priorityUnsubscribed (void) const { return _priority_unsubscribed; }
    void setPriorityUnsubscribed (int priority_unsubscribed) { _priority_unsubscribed = priority_unsubscribed; }

    std::string path (void) const { return _path; }
    void setPath (const std::string & path) { _path = path; }

    std::string filePath (void) const { return _file_path; }
    void setFilePath (const std::string file_path) { _file_path = file_path; }

    std::string iconFile (void) const { return _icon_file; }
    void setIconFile (const std::string icon_file) { _icon_file = icon_file; }

    std::string pkginfoFile (void) const { return _pkginfo_file; }
    void setPkginfoFile (const std::string & pkginfo_file) { _pkginfo_file = pkginfo_file; }

//    const std::list<char *> *distroTargets (void) const { return _distro_targets; }
//    void setDistroTargets (const std::list<char *> *distro_targets) { _distro_targets = distro_targets; }

    time_t lastUpdate (void) const { return _last_update; }
    void setLastUpdate (time_t last_update) { _last_update = last_update; }

    bool system (void) const { return _system; }
    void setSystem (bool system) { _system = system; }
    bool hidden (void) const { return _hidden; }
    void setHidden (bool hidden) { _hidden = hidden; }
    bool immutable (void) const { return _immutable; }
    void setImmutable (bool immutable) { _immutable = immutable; }

    //-----------------------------------------------------------------------

    bool isWildcard (void) const;

    virtual bool equals (const Channel & channel) const;
    virtual bool equals (Channel_constPtr channel) const;
    bool hasEqualId (const Channel & channel) const;
    bool hasEqualId (Channel_constPtr channel) const;

	//RCResItemSList *rc_channel_get_resItems (RCChannel *channel);

    // Distro target functions

    void addDistroTarget (const std::string & target);
    bool hasDistroTarget (const std::string & target) const;

    // Subscription management

    bool isSubscribed (void) const;
    void setSubscription (bool subscribed);

    int priorityParse (const std::string & priority_cptr) const;
    void setPriorities (int subd_priority, int unsubd_priority);
    int getPriority (bool is_subscribed) const;
};

///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////
#endif // ZYPP_SOLVER_TEMPORARY_CHANNEL_H
