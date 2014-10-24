#
# Tool to generate datavalidation report
#
# (c) 2014 Hewlett-Packard Nederland B.V.
#

import sys
import re
import os
import time
import shlex
import datetime
import argparse
import glob
from operator import itemgetter
import xml.etree.ElementTree as et
from xml.etree.ElementTree import tostring
from matplotlib.dates import date2num
from matplotlib.dates import DateFormatter
import matplotlib.ticker as mticker

from xml.dom import minidom

from matplotlib import rcParams
# rcParams['font.family'] = 'Garamond'

import matplotlib
# chose a non-GUI backend
matplotlib.use( 'Agg' )

import pylab

graph_size     = 8 
adj_hspace     = 0.05
adj_top        = 0.87
adj_bottom     = 0.09
adj_left       = 0.10
unit           = 'aantal'
scale_name     = 'ritten'
images_dir     = 'images_dir'

if not os.path.isdir (images_dir):
    try:
        os.mkdir (images_dir)
    except:
        print 'failed to make directory %s\n' % (images_dir)
        exit (1)
    
fp = open(os.path.join ('.', "rapport_") + 'datavalidatie' + ".txt",'w')

preface = """Datavalidatie
===============
HP Travel & Transportation
:lang: nl

== Inleiding

Dit rapport bevat de toelichting op de datavalidatie die wordt uitgevoerd na het importeren van alle gegevens in de Bliksem database. Deze datavalidatie dient voor het exporteren van de gegevens in de verschillende uitvoerformaten (IFF, GFTS) te worden uitgevoerd.
Dit rapport bevat informatie over:

[options="compact"]
* Volledigheid
* Correctheid
* Statistieken

Dit document is gegenereerd op {docdate}.

""" 

fp.write (preface);

fp.write ("""
== Maatschappij informatie

In onderstaande overzichten wordt het aantal ritten van een maatschappij uitgezet tegen de dagen. Hierdoor kan een indicatie worden gekregen over de volledigheid.

Bij het beoordelen van de overzichten dient men rekening te houden met:

[options="compact"]
* Feestdagen
* Weekend
* Schoolvakanties
* Wijzigingen in dienstregeling 
""")

tree = et.parse ('operator_journeys.xml')

doc = tree.getroot ()

e_window  =  doc.find ('window')
from_date = e_window.find ('fromdate').text
to_date   = e_window.find ('todate').text

heading = 'van ' + from_date + ' tot ' + to_date

for operator in doc.iter ('operator'):
   ocode =  operator.find ('operatorcode').text
   oname =  operator.find ('operatorname').text
   days =  operator.find ('days')

   print 'Processing operator %s' % (ocode,)

   date_list = []
   req_per_day = []

   for day in days.iter ('day'):
       e_date =  day.find ('date').text
       e_journeys =  day.find ('journeys').text
       date_list.append (datetime.datetime.strptime(e_date, "%Y-%m-%d"))
       req_per_day.append (int (e_journeys))

   fp.write ('=== Operator %s (%s)\n\n' % (oname, ocode))

   pylab.rcParams['figure.figsize'] = [graph_size + 1, graph_size]

   pylab.subplots_adjust (bottom = adj_bottom, top = adj_top, left=adj_left, hspace = adj_hspace)

   pylab.ylabel ('%s\n%s' % (unit, scale_name), rotation='horizontal')

   pylab.title (oname + ' (' + ocode + ') '  + heading)

   width = 0.60
   x = [date2num(date) for date in date_list]

   pylab.xticks (x)

   p1 = pylab.bar(x, req_per_day, width, color='#0096d6', edgecolor='#0096d6')

   ax = pylab.gca ()

   ax.spines ["top"].set_visible(False)
   ax.spines ["right"].set_visible(False)
   ax.get_xaxis().tick_bottom()
   ax.get_yaxis().tick_left()
   pylab.xlim(x[0],x[-1])
   ax.xaxis.set_major_formatter(DateFormatter('%m/%d/%Y'))
#   ax.autofmt_xdate ()

   mul7 = range (7, len (date_list), 7)
   ax.set_xticklabels(
        [date_list [i].strftime("%Y-%m-%d") for i in mul7], rotation=45
        )
   myLocator = mticker.MultipleLocator(7)
   ax.xaxis.set_major_locator(myLocator)

   file = ocode.replace (':', '=') + '_days' + '.svg'

   pylab.savefig ( os.path.join (images_dir,file), format='svg' )
   pylab.close ()
   fp.write ("""
[options="pgwide"]
image::%s['Maand overzicht', width="390", align="left"]\n""" % (os.path.join (images_dir,file)))

fp.write ("""
== Leverancier informatie

In onderstaande overzichten wordt het aantal ritten van een leverancier van de data uitgezet tegen de dagen. Hierdoor kan een indicatie worden gekregen over de volledigheid.

Bij het beoordelen van de overzichten dient men rekening te houden met:

[options="compact"]
* Feestdagen
* Weekend
* Schoolvakanties
* Wijzigingen in dienstregeling 

""")

tree = et.parse ('datasource_journeys.xml')

doc = tree.getroot ()

e_window  =  doc.find ('window')
from_date = e_window.find ('fromdate').text
to_date   = e_window.find ('todate').text

heading = 'van ' + from_date + ' tot ' + to_date

for datasource in doc.iter ('datasource'):
   ocode =  datasource.find ('datasourcecode').text
   oname =  datasource.find ('datasourcename').text
   days =  datasource.find ('days')

   print 'Processing datasource %s' % (ocode,)

   date_list = []
   req_per_day = []

   for day in days.iter ('day'):
       e_date =  day.find ('date').text
       e_journeys =  day.find ('journeys').text
       date_list.append (datetime.datetime.strptime(e_date, "%Y-%m-%d"))
       req_per_day.append (int (e_journeys))

   fp.write ('=== Leverancier data %s (%s)\n\n' % (oname, ocode))

   pylab.rcParams['figure.figsize'] = [graph_size + 1, graph_size]

   pylab.subplots_adjust (bottom = adj_bottom, top = adj_top, left=adj_left, hspace = adj_hspace)

   pylab.ylabel ('%s\n%s' % (unit, scale_name), rotation='horizontal')

   pylab.title (oname + ' (' + ocode + ') '  + heading)

   width = 0.60
   x = [date2num(date) for date in date_list]

   pylab.xticks (x)

   p1 = pylab.bar(x, req_per_day, width, color='#0096d6', edgecolor='#0096d6')

   ax = pylab.gca ()

   ax.spines ["top"].set_visible(False)
   ax.spines ["right"].set_visible(False)
   ax.get_xaxis().tick_bottom()
   ax.get_yaxis().tick_left()
   pylab.xlim(x[0],x[-1])
   ax.xaxis.set_major_formatter(DateFormatter('%m/%d/%Y'))
#   ax.autofmt_xdate ()

   mul7 = range (7, len (date_list), 7)
   ax.set_xticklabels(
        [date_list [i].strftime("%Y-%m-%d") for i in mul7], rotation=45
        )
   myLocator = mticker.MultipleLocator(7)
   ax.xaxis.set_major_locator(myLocator)

   file = 'ds_' + ocode + '_days' + '.svg'

   pylab.savefig ( os.path.join (images_dir,file), format='svg' )
   pylab.close ()
   fp.write ("""
[options="pgwide"]
image::%s['Maand overzicht', width="390", align="left"]\n""" % (os.path.join (images_dir,file)))


fp.close ()
