/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* Resolver_problems.cc
 *
 * Copyright (C) 2000-2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
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

#include <map>

#include "zypp/solver/temporary/ResItem.h"

#include "zypp/solver/detail/Resolver.h"
#include "zypp/solver/detail/ResolverContext.h"
#include "zypp/solver/detail/ResolverProblem.h"
#include "zypp/solver/detail/ProblemSolution.h"

#include "zypp/solver/detail/ResolverInfoChildOf.h"
#include "zypp/solver/detail/ResolverInfoConflictsWith.h"
#include "zypp/solver/detail/ResolverInfoContainer.h"
#include "zypp/solver/detail/ResolverInfoDependsOn.h"
#include "zypp/solver/detail/ResolverInfoMisc.h"
#include "zypp/solver/detail/ResolverInfoMissingReq.h"
#include "zypp/solver/detail/ResolverInfoNeededBy.h"
#include "zypp/solver/detail/ResolverInfoObsoletes.h"

#include "zypp/base/String.h"
#include "zypp/base/Logger.h"
#include "zypp/base/Gettext.h"

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////

using namespace std;

typedef map<ResItem_constPtr, ResolverInfo_Ptr> ProblemMap;

// set resolvables with errors

typedef struct {
    ProblemMap problems;
} ResItemCollector;


static void
collector_cb (ResolverInfo_Ptr info, void *data)
{
    ResItemCollector *collector = (ResItemCollector *)data;
    ResItem_constPtr resItem = info->affected();
    if (resItem != NULL) {
	collector->problems[resItem] = info;
    }
}


ResolverProblemList
Resolver::problems (void) const
{
    ResolverProblemList problems;

    if (_best_context) {
	MIL << "Valid solution found, no problems" << endl;
	return problems;
    }

    // collect all resolvables with error

    ResolverQueueList invalid = invalidQueues();
    MIL << invalid.size() << " invalid queues" << endl;

    if (invalid.empty()) {
	WAR << "No solver problems, other error" << endl;
    }

    ResolverContext_Ptr context = invalid.front()->context();

    ResItemCollector collector;
    context->foreachInfo (NULL, RESOLVER_INFO_PRIORITY_VERBOSE, collector_cb, &collector);

    for (ProblemMap::const_iterator iter = collector.problems.begin(); iter != collector.problems.end(); ++iter) {
	ResItem_constPtr resItem = iter->first;
	ResolverInfo_Ptr info = iter->second;
	string who = resItem->asString();
	string why;
	string details;
	switch (info->type()) {
	    case RESOLVER_INFO_TYPE_INVALID: {
		why = "Invalide information";
	    }
	    break;
	    case RESOLVER_INFO_TYPE_NEEDED_BY: {
		ResolverInfoNeededBy_constPtr needed_by = dynamic_pointer_cast<const ResolverInfoNeededBy>(info);
		why = "Is needed by " + needed_by->resItemsToString(true);
	    }
	    break;
	    case RESOLVER_INFO_TYPE_CONFLICTS_WITH: {
		ResolverInfoConflictsWith_constPtr conflicts_with = dynamic_pointer_cast<const ResolverInfoConflictsWith>(info);
		why = "Conflicts with " + conflicts_with->resItemsToString(true);
	    }
	    break;
	    case RESOLVER_INFO_TYPE_OBSOLETES: {
		ResolverInfoObsoletes_constPtr obsoletes = dynamic_pointer_cast<const ResolverInfoObsoletes>(info);
		why = "Obsoletes " + obsoletes->resItemsToString(true);
	    }
	    break;
	    case RESOLVER_INFO_TYPE_DEPENDS_ON: {
		ResolverInfoDependsOn_constPtr depends_on = dynamic_pointer_cast<const ResolverInfoDependsOn>(info);
		why = "Depends on " + depends_on->resItemsToString(true);
	    }
	    break;
	    case RESOLVER_INFO_TYPE_CHILD_OF: {				// unused
		ResolverInfoChildOf_constPtr child_of = dynamic_pointer_cast<const ResolverInfoChildOf>(info);
		why = "Child of";
	    }
	    break;
	    case RESOLVER_INFO_TYPE_MISSING_REQ: {
		ResolverInfoMissingReq_constPtr missing_req = dynamic_pointer_cast<const ResolverInfoMissingReq>(info);
		why = "Noone provides " + missing_req->capability().asString();
	    }
	    break;

	// from ResolverContext
	    case RESOLVER_INFO_TYPE_INVALID_SOLUTION: {			// Marking this resolution attempt as invalid.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_UNINSTALLABLE: {			// Marking p as uninstallable
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_REJECT_INSTALL: {			// p is scheduled to be installed, but this is not possible because of dependency problems.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_INSTALL_TO_BE_UNINSTALLED: {	// Can't install p since it is already marked as needing to be uninstalled
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_INSTALL_UNNEEDED: {			// Can't install p since it is does not apply to this system.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_INSTALL_PARALLEL: {			// Can't install p, since a resolvable of the same name is already marked as needing to be installed.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_INCOMPLETES: {			// This would invalidate p
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	// from QueueItemEstablish
	    case RESOLVER_INFO_TYPE_ESTABLISHING: {			// Establishing p
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	// from QueueItemInstall
	    case RESOLVER_INFO_TYPE_INSTALLING: {			// Installing p
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_UPDATING: {				// Updating p
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_SKIPPING: {				// Skipping p, already installed
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	// from QueueItemRequire
	    case RESOLVER_INFO_TYPE_NO_OTHER_PROVIDER: {		// There are no alternative installed providers of c [for p]
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_NO_PROVIDER: {			// There are no installable providers of c [for p]
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_NO_UPGRADE: {			// Upgrade to q to avoid removing p is not possible.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_UNINSTALL_PROVIDER: {		// p provides c but is scheduled to be uninstalled
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_PARALLEL_PROVIDER: {		// p provides c but another version is already installed
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_NOT_INSTALLABLE_PROVIDER: {		// p provides c but is uninstallable
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_LOCKED_PROVIDER: {			// p provides c but is locked
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_CANT_SATISFY: {			// Can't satisfy requirement c
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	// from QueueItemUninstall
	    case RESOLVER_INFO_TYPE_UNINSTALL_TO_BE_INSTALLED: {	// p is to-be-installed, so it won't be unlinked.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_UNINSTALL_INSTALLED: {		// p is required by installed, so it won't be unlinked.
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_UNINSTALL_LOCKED: {			// cant uninstall, its locked
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	// from QueueItemConflict
	    case RESOLVER_INFO_TYPE_CONFLICT_CANT_INSTALL: {		// to-be-installed p conflicts with q due to c
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	    case RESOLVER_INFO_TYPE_CONFLICT_UNINSTALLABLE: {		// uninstalled p is marked uninstallable it conflicts [with q] due to c
		ResolverInfoMisc_constPtr misc_info = dynamic_pointer_cast<const ResolverInfoMisc>(info);
		why = misc_info->message();
	    }
	    break;
	}
	ResolverProblem_Ptr problem = new ResolverProblem (why, details);
	problems.push_back (problem);
    }
    if (problems.empty()) {
	context->spewInfo();
    }
    return problems;
}


void
Resolver::applySolutions (const ProblemSolutionList & solutions)
{
    return;
}

///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////

