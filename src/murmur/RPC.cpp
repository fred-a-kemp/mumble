/* Copyright (C) 2005-2009, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Server.h"
#include "Version.h"

void Server::setPlayerState(Player *pPlayer, Channel *cChannel, bool mute, bool deaf, bool suppressed) {
	bool changed = false;

	if (deaf)
		mute = true;
	if (! mute)
		deaf = false;

	if ((pPlayer->bDeaf != deaf) && (deaf || (!deaf && mute))) {
		pPlayer->bDeaf = deaf;
		pPlayer->bMute = mute;
		MessagePlayerDeaf mpd;
		mpd.uiSession = 0;
		mpd.uiVictim=pPlayer->uiSession;
		mpd.bDeaf = deaf;
		sendAll(&mpd);
		changed = true;
	} else if ((pPlayer->bDeaf != deaf) || (pPlayer->bMute != mute)) {
		pPlayer->bDeaf = deaf;
		pPlayer->bMute = mute;

		MessagePlayerMute mpm;
		mpm.uiSession = 0;
		mpm.uiVictim=pPlayer->uiSession;
		mpm.bMute=mute;
		sendAll(&mpm);
		changed = true;
	}

	if (cChannel != pPlayer->cChannel) {
		playerEnterChannel(pPlayer, cChannel);
		MessagePlayerMove mpm;
		mpm.uiSession = 0;
		mpm.uiVictim = pPlayer->uiSession;
		mpm.iChannelId = cChannel->iId;
		sendAll(&mpm);
		changed = true;
	}

	emit playerStateChanged(pPlayer);
}

bool Server::setChannelState(Channel *cChannel, Channel *cParent, const QString &qsName, const QSet<Channel *> &links) {
	bool changed = false;
	bool updated = false;

	if (cChannel->qsName != qsName) {
		cChannel->qsName = qsName;
		MessageChannelRename mcr;
		mcr.uiSession = 0;
		mcr.iId = cChannel->iId;
		mcr.qsName = cChannel->qsName;
		sendAll(&mcr);
		updated = true;
		changed = true;
	}

	if ((cParent != cChannel) && (cParent != cChannel->cParent)) {
		Channel *p = cParent;
		while (p) {
			if (p == cChannel)
				return false;
			p = p->cParent;
		}

		cChannel->cParent->removeChannel(cChannel);
		cParent->addChannel(cChannel);

		MessageChannelMove mcm;
		mcm.uiSession = 0;
		mcm.iId = cChannel->iId;
		mcm.iParent = cParent->iId;
		sendAll(&mcm);

		updated = true;
		changed = true;
	}

	const QSet<Channel *> &oldset = cChannel->qsPermLinks;

	if (links != oldset) {
		// Remove
		foreach(Channel *l, links) {
			if (! links.contains(l)) {
				removeLink(cChannel, l);

				MessageChannelLink mcl;
				mcl.uiSession = 0;
				mcl.iId = cChannel->iId;
				mcl.qlTargets << l->iId;
				mcl.ltType = MessageChannelLink::Unlink;
				sendAll(&mcl);
			}
		}

		// Add
		foreach(Channel *l, links) {
			if (! oldset.contains(l)) {
				addLink(cChannel, l);

				MessageChannelLink mcl;
				mcl.uiSession = 0;
				mcl.iId = cChannel->iId;
				mcl.qlTargets << l->iId;
				mcl.ltType = MessageChannelLink::Link;
				sendAll(&mcl);
			}
		}

		changed = true;
	}

	if (updated)
		updateChannel(cChannel);
	if (changed)
		emit channelStateChanged(cChannel);

	return true;
}

void Server::sendTextMessage(Channel *cChannel, User *pPlayer, bool tree, const QString &text) {
	MessageTextMessage mtm;
	mtm.uiSession = 0;
	mtm.qsMessage = text;
	if (pPlayer) {
		mtm.uiVictim = pPlayer->uiSession;
		mtm.iChannel = -1;
		mtm.bTree = false;
		sendMessage(pPlayer, &mtm);
	} else {
		mtm.uiVictim = 0;
		mtm.iChannel = cChannel->iId;
		mtm.bTree = tree;

		QSet<Channel *> chans;
		QQueue<Channel *> q;
		q << cChannel;
		chans.insert(cChannel);
		Channel *c;

		if (tree) {
			while (! q.isEmpty()) {
				c = q.dequeue();
				chans.insert(c);
				foreach(c, c->qlChannels)
				q.enqueue(c);
			}
		}
		foreach(c, chans) {
			foreach(Player *p, c->qlPlayers)
			sendMessage(static_cast<User *>(p), &mtm);
		}
	}
}

void Meta::getVersion(int &major, int &minor, int &patch, QString &string) {
	string = QLatin1String(MUMBLE_RELEASE);
	major = minor = patch = 0;
	QRegExp rx(QLatin1String("(\\d+)\\.(\\d+)\\.(\\d+)"));
	if (rx.exactMatch(QLatin1String(MUMTEXT(MUMBLE_VERSION_STRING)))) {
		major = rx.cap(1).toInt();
		minor = rx.cap(2).toInt();
		patch = rx.cap(3).toInt();
	}
}
