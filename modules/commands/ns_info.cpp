/* NickServ core functions
 *
 * (C) 2003-2014 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"
#include "modules/ns_info.h"
#include "modules/ns_set.h"

class CommandNSInfo : public Command
{
	EventHandlers<Event::NickInfo> &onnickinfo;

 public:
	CommandNSInfo(Module *creator, EventHandlers<Event::NickInfo> &event) : Command(creator, "nickserv/info", 0, 2), onnickinfo(event)
	{
		this->SetDesc(_("Displays information about a given nickname"));
		this->SetSyntax(_("[\037nickname\037]"));
		this->AllowUnregistered(true);
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{

		const Anope::string &nick = params.size() ? params[0] : (source.nc ? source.nc->display : source.GetNick());
		NickServ::Nick *na = NickServ::FindNick(nick);
		bool has_auspex = source.HasPriv("nickserv/auspex");

		if (!na)
		{
			if (BotInfo::Find(nick, true))
				source.Reply(_("\002{0}\002 is part of this Network's Services."), nick);
			else
				source.Reply(_("\002{0}\002 isn't registered."), nick);
		}
		else
		{
			bool nick_online = false, show_hidden = false;

			/* Is the real owner of the nick we're looking up online? -TheShadow */
			User *u2 = User::Find(na->nick);
			if (u2 && u2->Account() == na->nc)
			{
				nick_online = true;
				na->last_seen = Anope::CurTime;
			}

			if (has_auspex || na->nc == source.GetAccount())
				show_hidden = true;

			source.Reply(_("\002{0}\002 is \002{1}\002"), na->nick, na->last_realname);

			if (na->nc->HasExt("UNCONFIRMED"))
				source.Reply(_("\002{0}\002 has not confirmed their account."), na->nick);

			if (na->nc->IsServicesOper() && (show_hidden || !na->nc->HasExt("HIDE_STATUS")))
				source.Reply(_("\002{0}\002 is a Services Operator of type \002{0}\002."), na->nick, na->nc->o->ot->GetName());

			InfoFormatter info(source.nc);

			if (nick_online)
			{
				bool shown = false;
				if (show_hidden && !na->last_realhost.empty())
				{
					info[_("Online from")] = na->last_realhost;
					shown = true;
				}
				if ((show_hidden || !na->nc->HasExt("HIDE_MASK")) && (!shown || na->last_usermask != na->last_realhost))
					info[_("Online from")] = na->last_usermask;
				else
					source.Reply(_("\002{0}\002 is currently online."), na->nick);
			}
			else
			{
				Anope::string shown;
				if (show_hidden || !na->nc->HasExt("HIDE_MASK"))
				{
					info[_("Last seen address")] = na->last_usermask;
					shown = na->last_usermask;
				}

				if (show_hidden && !na->last_realhost.empty() && na->last_realhost != shown)
					info[_("Last seen address")] = na->last_realhost;
			}

			info[_("Registered")] = Anope::strftime(na->time_registered, source.GetAccount());

			if (!nick_online)
				info[_("Last seen")] = Anope::strftime(na->last_seen, source.GetAccount());

			if (!na->last_quit.empty() && (show_hidden || !na->nc->HasExt("HIDE_QUIT")))
				info[_("Last quit message")] = na->last_quit;

			if (!na->nc->email.empty() && (show_hidden || !na->nc->HasExt("HIDE_EMAIL")))
				info[_("Email address")] = na->nc->email;

			if (show_hidden)
			{
				if (na->HasVhost())
				{
					if (IRCD->CanSetVIdent && !na->GetVhostIdent().empty())
						info[_("VHost")] = na->GetVhostIdent() + "@" + na->GetVhostHost();
					else
						info[_("VHost")] = na->GetVhostHost();
				}
			}

			this->onnickinfo(&Event::NickInfo::OnNickInfo, source, na, info, show_hidden);

			std::vector<Anope::string> replies;
			info.Process(replies);

			for (unsigned i = 0; i < replies.size(); ++i)
				source.Reply(replies[i]);
		}
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Displays information about the given nickname, such as\n"
				"the nick's owner, last seen address and time, and nick\n"
				"options. If no nick is given, and you are identified,\n"
				"your account name is used, else your current nickname is\n"
				"used."));

		return true;
	}
};


class CommandNSSetHide : public Command
{
 public:
	CommandNSSetHide(Module *creator, const Anope::string &sname = "nickserv/set/hide", size_t min = 2) : Command(creator, sname, min, min + 1)
	{
		this->SetDesc(_("Hide certain pieces of nickname information"));
		this->SetSyntax("{EMAIL | STATUS | USERMASK | QUIT} {ON | OFF}");
	}

	void Run(CommandSource &source, const Anope::string &user, const Anope::string &param, const Anope::string &arg)
	{
		if (Anope::ReadOnly)
		{
			source.Reply(_("Services are in read-only mode."));
			return;
		}

		const NickServ::Nick *na = NickServ::FindNick(user);
		if (!na)
		{
			source.Reply(_("\002{0}\002 isn't registered."), user);
			return;
		}
		NickServ::Account *nc = na->nc;

		EventReturn MOD_RESULT = Event::OnSetNickOption(&Event::SetNickOption::OnSetNickOption, source, this, nc, param);
		if (MOD_RESULT == EVENT_STOP)
			return;

		const char *onmsg, *offmsg, *flag;

		if (param.equals_ci("EMAIL"))
		{
			flag = "HIDE_EMAIL";
			onmsg = _("The \002e-mail address\002 of \002{0}\002 will now be \002hidden\002.");
			offmsg = _("The \002e-mail address\002 of \002{0}\002 will now be \002shown\002.");
		}
		else if (param.equals_ci("USERMASK"))
		{
			flag = "HIDE_MASK";
			onmsg = _("The \002last seen host mask\002 of \002{0}\002 will now be \002hidden\002.");
			offmsg = _("The \002last seen host mask\002 of \002{0}\002 will now be \002shown\002.");
		}
		else if (param.equals_ci("STATUS"))
		{
			flag = "HIDE_STATUS";
			onmsg = _("The \002services operator status\002 of \002{0}\002 will now be \002hidden\002.");
			offmsg = _("The \002services operator status\002 of \002{0}\002 will now be \002shown\002.");
		}
		else if (param.equals_ci("QUIT"))
		{
			flag = "HIDE_QUIT";
			onmsg = _("The \002last quit message\002 of \002{0}\002 will now be \002hidden\002.");
			offmsg = _("The \002last quit message\002 of \002{0}\002 will now be \002shown\002.");
		}
		else
		{
			this->OnSyntaxError(source, "HIDE");
			return;
		}

		if (arg.equals_ci("ON"))
		{
			Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source, this) << "to change hide " << param.upper() << " to " << arg.upper() << " for " << nc->display;
			nc->Extend<bool>(flag);
			source.Reply(onmsg, nc->display, source.service->nick);
		}
		else if (arg.equals_ci("OFF"))
		{
			Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source, this) << "to change hide " << param.upper() << " to " << arg.upper() << " for " << nc->display;
			nc->Shrink<bool>(flag);
			source.Reply(offmsg, nc->display, source.service->nick);
		}
		else
			this->OnSyntaxError(source, "HIDE");
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		this->Run(source, source.nc->display, params[0], params[1]);
	}

	bool OnHelp(CommandSource &source, const Anope::string &) override
	{
		source.Reply(_("Allows you to prevent certain pieces of information from being displayed when someone does a %s \002INFO\002 on you." //XXX
		               " You can hide the e-mail address (\002EMAIL\002), last seen hostmask (\002USERMASK\002), the services access status (\002STATUS\002) and last quit message (\002QUIT\002)."
				"The second parameter specifies whether the information should\n"
				"be displayed (\002OFF\002) or hidden (\002ON\002)."), source.service->nick);
		return true;
	}
};

class CommandNSSASetHide : public CommandNSSetHide
{
 public:
	CommandNSSASetHide(Module *creator) : CommandNSSetHide(creator, "nickserv/saset/hide", 3)
	{
		this->SetSyntax(_("\037nickname\037 {EMAIL | STATUS | USERMASK | QUIT} {ON | OFF}"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		this->ClearSyntax();
		this->Run(source, params[0], params[1], params[2]);
	}

	bool OnHelp(CommandSource &source, const Anope::string &) override
	{
		source.Reply(_("Allows you to prevent certain pieces of information from being displayed when someone does a %s \002INFO\002 on the \037nickname\037. "
		               " You can hide the e-mail address (\002EMAIL\002), last seen hostmask (\002USERMASK\002), the services access status (\002STATUS\002) and last quit message (\002QUIT\002)."
		               " The second parameter specifies whether the information should be displayed (\002OFF\002) or hidden (\002ON\002)."),
		               source.service->nick);
		return true;
	}
};

class NSInfo : public Module
{
	CommandNSInfo commandnsinfo;

	CommandNSSetHide commandnssethide;
	CommandNSSASetHide commandnssasethide;

	EventHandlers<Event::NickInfo> onnickinfo;

	SerializableExtensibleItem<bool> hide_email, hide_usermask, hide_status, hide_quit;

 public:
	NSInfo(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR)
		, commandnsinfo(this, onnickinfo)
		, commandnssethide(this)
		, commandnssasethide(this)
		, onnickinfo(this, "OnNickInfo")
		, hide_email(this, "HIDE_EMAIL")
		, hide_usermask(this, "HIDE_MASK")
		, hide_status(this, "HIDE_STATUS")
		, hide_quit(this, "HIDE_QUIT")
	{

	}
};

MODULE_INIT(NSInfo)
