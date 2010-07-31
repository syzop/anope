/* HostServ core functions
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

class CommandHSOff : public Command
{
 public:
	CommandHSOff() : Command("OFF", 0, 0)
	{
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		NickAlias *na = findnick(u->nick);

		if (!na || !na->hostinfo.HasVhost())
			notice_lang(Config.s_HostServ, u, HOST_NOT_ASSIGNED);
		else
		{
			ircdproto->SendVhostDel(u);
			notice_lang(Config.s_HostServ, u, HOST_OFF);
		}

		return MOD_CONT;
	}

	bool OnHelp(User *u, const Anope::string &subcommand)
	{
		notice_help(Config.s_HostServ, u, HOST_HELP_OFF);
		return true;
	}

	void OnServHelp(User *u)
	{
		notice_lang(Config.s_HostServ, u, HOST_HELP_CMD_OFF);
	}
};

class HSOff : public Module
{
 public:
	HSOff(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(HostServ, new CommandHSOff());
	}
};

MODULE_INIT(HSOff)