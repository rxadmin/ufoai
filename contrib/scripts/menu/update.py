#!/usr/bin/env python

#
# @brief some example to update menu_*.ufo files
# @license Public domaine
# @note tested with Python 2.4.1
#

import os, sys, re
import os.path 
from UFOMenuAPI import Root


def renameMenuOriginToPos():
	root = Root()
	root.loadAll()
	#root.loadFile('base/ufos/menu_options.ufo')
	
	print '-----'
	
	for menu in root.nodes.child:
		if not menu.existsParam("origin"):
			continue
		print menu.name
		menu.renameMenuParam("origin", "pos")
	
	root.save()


def searchWindowpanelWithoutParentPosSize():
	root = Root()
	root.loadAll()
	#root.loadFile('base/ufos/menu_options.ufo')
	
	print '-----'
	
	for menu in root.nodes.child:
		if menu.existsParam("pos") and menu.existsParam("size"):
			continue
		for node in menu.child:
			if node.behaviour != "windowpanel":
				continue
				
			print menu.filename + ' > menu ' + menu.name + ' have no pos or size'


def fixeWindowpanelUnderWindow():
	root = Root()
	#root.loadAll()
	#root.loadFile('base/ufos/menu_singleplayer.ufo')
	#root.loadFile('base/ufos/menu_aircraft.ufo')
	#root.loadFile('base/ufos/menu_geoscape.ufo')
	#root.loadFile('base/ufos/menu_irc.ufo')
	#root.loadFile('base/ufos/menu_lostwon.ufo')
	#root.loadFile('base/ufos/menu_main.ufo')
	root.loadFile('base/ufos/menu_multiplayer.ufo')
	#root.loadFile('base/ufos/menu_options.ufo')
	#root.loadFile('base/ufos/menu_singleplayer.ufo')

	print '-----'
	
	for menu in root.nodes.child:
		if not menu.existsParam("pos") or not menu.existsParam("size"):
			continue
		print 'menu ' + menu.name
		
		# find 	windowpanel
		windowpanel = None
		for node in menu.child:
			if node.behaviour != "windowpanel":
				continue
			windowpanel = node
			break
		
		# menu without background
		if windowpanel == None:
			break

		# all right
		if windowpanel.pos == (0, 0):
			print 'menu ' + menu.name + ' (' + menu.filename + ') at right place'
			break
		
		dx, dy = windowpanel.pos
		x, y = menu.pos[0] + dx, menu.pos[1] + dy
		menu.updateParam("pos", '"' + str(x) + " " + str(y) + '"')
		
		for node in menu.child:
			if not node.existsParam("pos"):
				continue
				
			x, y = node.pos[0] - dx, node.pos[1] - dy
			node.updateParam("pos", '"' + str(x) + " " + str(y) + '"')

	root.save()

def moveOptionMenuContent():
	root = Root()
	root.loadFile('base/ufos/menu_options.ufo')

	print '-----'
	
	for menu in root.nodes.child:
		if menu.name not in ["options_video", "options_sound", "options_keys", "options_pause", "options_game", "options_input"]:
			continue
		print 'menu ' + menu.name
		
		dx, dy = 415, 0
		for node in menu.child:
			if not node.existsParam("pos"):
				continue
				
			x, y = node.pos[0] - dx, node.pos[1] - dy
			node.updateParam("pos", '"' + str(x) + " " + str(y) + '"')

	root.save()

def findNonULCheckBox():
	root = Root()
	root.loadAll()

	print '-----'
	
	for menu in root.nodes.child:
		print 'menu ' + menu.name + " " + menu.filename

		for node in menu.child:
			if node.behaviour != "checkbox":
				continue
			if not node.existsParam("align"):
				continue
			align = node.param["align"]
			## @todo the API must remove quotes 
			if align == "ul" or align == "\"ul\"":
				continue
		
			print node.name + " align=" + align

def moveMultiplayerMenuContent():
	root = Root()
	root.loadFile('base/ufos/menu_multiplayer.ufo')

	print '-----'
	
	for menu in root.nodes.child:
		if menu.name not in ["mp_serverbrowser", "mp_create_server", "mp_team"]:
			continue
		print 'menu ' + menu.name
		
		dx, dy = 415, 0
		for node in menu.child:
			if not node.existsParam("pos"):
				continue
				
			x, y = node.pos[0] - dx, node.pos[1] - dy
			node.updateParam("pos", '"' + str(x) + " " + str(y) + '"')

	root.save()

##
# @brief move the reserch table where we want

##
# Move a range of node into a new position
def moveFullCol(menu, prefix, number, newpos, posref, dexY):
	print "----- " + prefix + " -----"
	dec = menu.childname[prefix + "0"].pos
	dec = dec[0] - posref[0], dec[1] - posref[1]

	for i in xrange(0, number + 1):
		x, y = newpos[0] + dec[0], newpos[1] + dec[1] + i * dexY
		menu.childname[prefix + str(i)].updateParam("pos", '"' + str(x) + " " + str(y) + '"')


def moveFullPanel(menu, nodes, newpos, posref):
	print "----- " + "moveFullPanel" + " -----"
	dec = newpos[0] - posref[0], newpos[1] - posref[1]

	for n in nodes:
		x, y = menu.childname[n].pos
		x, y = x + dec[0], y + dec[1]
		menu.childname[n].updateParam("pos", '"' + str(x) + " " + str(y) + '"')
		

# @note update newpos to set a new position
def moveResearchTable():
	root = Root()
	root.loadFile('base/ufos/menu_research.ufo')

	print '-----'

	menu = root.nodes.childname["research"]
	posref = menu.childname["img_status0"].pos
	newpos = 50, 113
	
	moveFullCol(menu, "img_status", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_item", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_assigned", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_available", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_max", 27, newpos, posref, 25)
	moveFullCol(menu, "bt_rs_change", 27, newpos, posref, 25)	

	root.save()

def moveHireTable():
	root = Root()
	root.loadFile('base/ufos/menu_hire.ufo')
	print '-----'

	menu = root.nodes.childname["employees"]
	posref = menu.childname["bt_employee0"].pos
	newpos = 325, 180
	
	moveFullCol(menu, "bt_employee", 18, newpos, posref, 38)

	root.save()

def moveHirePanel():
	root = Root()
	root.loadFile('base/ufos/menu_hire.ufo')
	print '-----'

	nodes = ['name','bt_edit',
		'rank_img','rank_lbl',
		'pwr_lbl','pwr_val','pwr_brd','pwr_bar',
		'spd_lbl','spd_val','spd_bdr','spd_bar',
		'acc_lbl','acc_val','acc_bdr','acc_bar',
		'mnd_lbl','mnd_val','mnd_bdr','mnd_bar',
		'cls_lbl','cls_val','cls_bdr','cls_bar',
		'hvy_lbl','hvy_val','hvy_bdr','hvy_bar',
		'ass_lbl','ass_val','ass_bdr','ass_bar',
		'snp_lbl','snp_val','snp_bdr','snp_bar',
		'exp_lbl','exp_val','exp_bdr','exp_bar',
		'hp_lbl','hp_val','hp_bdr','hp_bar',
		'missions_lbl','missions_val','kills_lbl','kills_val'
	]

	menu = root.nodes.childname["employees"]
	posref = menu.childname["rank_lbl"].pos
	newpos = 485, 383
	
	moveFullPanel(menu, nodes, newpos, posref)

	root.save()

def renameTextNodeAlignProperty():
	root = Root()
	root.loadAll()

	print '-----'
	
	for menu in root.nodes.child:
		print 'menu ' + menu.name
		for node in menu.child:
			if node.behaviour != "text":
				continue
			if not node.existsParam("align"):
				continue
			print '  ' + node.name
			if not node.existsParam("size") and not node.existsParam("width"):
				print '    no size!'
			node.renameParam('align', 'textalign')

	root.save()

def fixStringNodeAlignProperty():
	root = Root()
	root.loadAll()

	print '-----'
	
	for menu in root.nodes.child:
		print 'menu ' + menu.name
		for node in menu.child:
			if node.behaviour != "string":
				continue
			if not node.existsParam("align"):
				continue
			if not node.existsParam("size"):
				continue
			
			valign = node.getParam("align")[0]
			halign = node.getParam("align")[1]
			
			x,y = node.pos
			size = node.size

			if halign == 'r':
				x = x - size[0]
			elif halign == 'c':
				x = x - long(size[0]/2)
			elif halign == 'l':
				pass

			if valign == 'l':
				y = y - size[1]
			elif valign == 'c':
				y = y - long(size[1]/2)
			elif valign == 'u':
				pass

			node.updateParam('pos', '"' + str(x) + " " + str(y) + '"')
			#node.updateParam('align', '##########')
			node.renameParam('align', 'textalign')
			print ' #FIX  ' + node.name + '  ' + node.behaviour

	root.save()

def checkAlign():
	root = Root()
	root.loadAll()

	behaviours = {}

	print '-----'
	
	for menu in root.nodes.child:
		print 'menu ' + menu.name
		for node in menu.child:
			if not node.existsParam('align'):
				continue
			
			if not node.behaviour in behaviours:
				behaviours[node.behaviour] = {'total':0}
			align = node.getParam('align')
			if not align in behaviours[node.behaviour]:
				behaviours[node.behaviour][align] = 0
			
			behaviours[node.behaviour]['total'] = behaviours[node.behaviour]['total'] + 1
			behaviours[node.behaviour][align] = behaviours[node.behaviour][align] + 1

			if node.existsParam('size'):
				print ' #  ' + node.name + '  ' + node.behaviour + '  ' + node.getParam('align')
			#print '  ' + node.name + ' ' + str(node.getParam('align'))
	
	print '-' * 20
	for b in behaviours:
		print b
		for i in behaviours[b]: print '\t' + i + '\t' + str(behaviours[b][i])

if __name__ == "__main__":
	#fixStringNodeAlignProperty()
	checkAlign()
	#root = Root()
	#root.loadAll()
	#root.loadFile('base/ufos/menu_team.ufo')
	pass
