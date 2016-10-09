// This file is part of flobby (GPL v2 or later), see the LICENSE file

#include "UserList.h"
#include "ITabs.h"
#include "PopupMenu.h"
#include "TextFunctions.h"

#include "model/Model.h"
#include "log/Log.h"

#include <FL/Fl.H>
#include "FL/fl_ask.H"
#include <boost/bind.hpp>

UserList::UserList(int x, int y, int w, int h, Model & model, ITabs & iTabs, bool savePrefs):
    StringTable(x, y, w, h, "UserList", { {"name",10}, {"status",4} }, 0 /* sort on name by default */, savePrefs),
    model_(model),
    iTabs_(iTabs)
{
    connectRowClicked( boost::bind(&UserList::userClicked, this, _1, _2) );
    connectRowDoubleClicked( boost::bind(&UserList::userDoubleClicked, this, _1, _2) );

    model_.connectUserChanged( boost::bind(&UserList::userChanged, this, _1) );
}

void UserList::add(User const & user)
{
    addRow(makeRow(user));
}

void UserList::add(std::string const & userName)
{
    User const & user = model_.getUser(userName);
    add(user);
}

void UserList::remove(std::string const & userName)
{
    removeRow(userName);
}

StringTableRow UserList::makeRow(User const & user)
{
    return StringTableRow( user.name(),
        {
            user.name(),
            statusString(user)
        } );
}

std::string UserList::statusString(User const & user)
{
    std::ostringstream oss;
    oss << (user.status().bot() ? "B" : "");
    oss << (user.joinedBattle() != -1 ? "J" : "");
    oss << (user.status().inGame() ? "G" : "");
    oss << (user.status().away() ? "A" : "");
    return oss.str();
}

void UserList::userChanged(User const & user)
{
    try
    {
        updateRow(makeRow(user)); // throws if row not found
    }
    catch (std::runtime_error & e)
    {
        // ignore
    }
}

void UserList::userClicked(int rowIndex, int button)
{
    if (button == FL_RIGHT_MOUSE)
    {
        StringTableRow const & row = getRow(static_cast<std::size_t>(rowIndex));

        std::string const userName = row.id_;
        User const * user = 0;
        PopupMenu menu;

        try
        {
            user = &model_.getUser(row.id_);
        }
        catch (std::invalid_argument const & e)
        {
            LOG(ERROR)<< e.what();
            return;
        }

        std::string const userInfo = user->info();
        menu.add(userInfo, 1);
        menu.add("Open chat", 2);

        int const battleId = user->joinedBattle();

        if (battleId != -1)
        {
            try
            {
                Battle const& battle = model_.getBattle(battleId);
                std::string joinText = "Join " + battle.title();
                menu.add(joinText, 3);
            }
            catch (std::invalid_argument const& e)
            {
                LOG(WARNING) << e.what();
            }
        }

        std::string const zkAccountID = user->zkAccountID();
        if (!zkAccountID.empty()) {
            menu.add("Open user web page", 4);
        }

        if (menu.size() > 0)
        {
            int const id = menu.show();
            switch (id)
            {
            case 1:
                Fl::copy(userInfo.c_str(), userInfo.size(), 1 /* clipboard */);
                break;
            case 2:
                try
                {
                    model_.getUser(userName); // to make sure user still exist
                    iTabs_.openPrivateChat(userName);
                }
                catch (std::invalid_argument const & e)
                {
                    LOG(WARNING)<< e.what();
                }
                break;

            case 3:
                try
                {
                    Battle const& battle = model_.getBattle(battleId);
                    if (battle.passworded())
                    {
                        char const * password = fl_input("Enter battle password");
                        if (password != NULL)
                        {
                            model_.joinBattle(battleId, password);
                        }
                    }
                    else
                    {
                        model_.joinBattle(battleId);
                    }
                }
                catch (std::invalid_argument const & e)
                {
                    LOG(WARNING)<< e.what();
                }
                break;

            case 4: {
                std::string const link("http://zero-k.info/Users/Detail/" + zkAccountID);
                flOpenUri(link);
                break;
            }
            }
        }
    }
}

void UserList::userDoubleClicked(int rowIndex, int button)
{
    StringTableRow const & row = getRow(static_cast<std::size_t>(rowIndex));
    iTabs_.openPrivateChat(row.id_);
}

std::string UserList::completeUserName(std::string const& text, std::string const& ignore)
{
    std::string fullUserName;

    if (text.empty())
    {
        LOG(DEBUG)<< "ignored trying to complete empty string";
        return fullUserName;
    }

    std::vector<std::string> userNames;

    for (int i=0; i<rows(); ++i)
    {
        StringTableRow const & row = getRow(static_cast<std::size_t>(i));
        try
        {
            User const & user = model_.getUser(row.data_[0]);
            userNames.push_back(row.data_[0]);
        }
        catch (std::invalid_argument const & e)
        {
            LOG(WARNING)<< "unexpected exception: "<< e.what();
        }
    }

    auto const match = findMatch(userNames, text, ignore);
    if (!match.empty())
    {
        fullUserName = match;
    }

    return fullUserName;
}
