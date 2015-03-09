#using python fuzzy_A1.py queriesAndResults.txt

import sys, urllib2, json, time, subprocess, os, commands,signal, argparse

sys.path.insert(0, 'srch2lib')
import test_lib

numberOfFacetFields= 3;

#the function of checking the results
def checkResult(query, responseJsonAll,resultValue, facetResultValue):
    responseJson = responseJsonAll['results']
    isPass=1
    if  len(responseJson) == len(resultValue):
         for i in range(0, len(resultValue)):
                #print response_json['results'][i]['record']['id']
            if (resultValue.count(responseJson[i]['record']['id']) != 1):
                isPass=0
                print query+' test failed'
                print 'query results||given results'
                print 'number of results:'+str(len(responseJson))+'||'+str(len(resultValue))
                for i in range(0, len(responseJson)):
                    print responseJson[i]['record']['id']+'||'+resultValue[i]
                break
    else:
        isPass=0
        print query+' test failed'
        print 'query results||given results'
        print 'number of results:'+str(len(responseJson))+'||'+str(len(resultValue))
        maxLen = max(len(responseJson),len(resultValue))
        for i in range(0, maxLen):
            if i >= len(resultValue):
                 print responseJson[i]['record']['id']+'||'
            elif i >= len(responseJson):
                 print '  '+'||'+resultValue[i]
            else:
                 print responseJson[i]['record']['id']+'||'+resultValue[i]

    isPass = checkFacetResults(query , responseJsonAll['facets'] , facetResultValue)

    if isPass == 1:
        print  query+' test pass'
        return 0
    return 1

# check if the faceted results are as expected
# Note: Query parameter &rows=1 prevents the number of values for any facet category from every
# being more than 1.
# If ranking ever changes, the record used to calculate facet counts will also change and
# this check will likely begin failing.
def checkFacetResults(query, responseJson, resultValue):
   if len(responseJson) != len(resultValue):
      return False
   for i in range(0,len(responseJson)):
      facet_line = ''
      facet_field_name = responseJson[i]['facet_field_name']
      facet_line = facet_line + facet_field_name + '||'
      facet_info = responseJson[i]['facet_info']
      for j in range(0,len(facet_info)):
         facet_line = facet_line + facet_info[j]['category_name'] + ',' + str(facet_info[j]['category_value']) + '|'
      if resultValue[i] != facet_line:
         print 'Expected facet results: ' + resultValue[i]
         print 'vs. Actual facet results: ' + facet_line
         print 'Facet test failed'
         return False
   return True

facetParamNameTuple= ('start', 'end', 'gap')

#prepare a facet parameter for the query based on list entry:
def prepareFacet(fieldTuple):
    query = 'facet.'
    query =  query + ('field' if (len(fieldTuple) == 1) else 'range')
    query = query + '=' + fieldTuple[0]
    i=0
    for value in fieldTuple[1:]:
        query = query + '&f.' + fieldTuple[0] + '.facet.'
        query = query + facetParamNameTuple[i]
        i= i + 1
        query = query + '=' + value
    return query

#prepare the query based on the valid syntax
def prepareQuery(queryKeywords, facetedFields):
    query = ''
    #################  prepare main query part
    query = query + 'q='
    # local parameters
    query = query + '%7BdefaultPrefixComplete=COMPLETE%7D'
    # keywords section
    for i in range(0, len(queryKeywords)):
        if i == (len(queryKeywords)-1):
            query=query+queryKeywords[i]+'*' # last keyword prefix
        else:
            query=query+queryKeywords[i]+'%20AND%20'

    ################# fuzzy parameter
    query = query + '&fuzzy=true'
    ################# facet parameters
    query = query + '&facet=true'

    for field in facetedFields:
        query = query + '&' + prepareFacet(field)
    ################# rows parameter
    query = query + '&rows=1&searchType=getAll'
    print 'Query : ' + query
    ##################################
    return query

def testFacetedSearch(f_in , f_facet, binary_path):
    # Start the engine server
    args = [ binary_path, './faceted_search/conf.xml', './faceted_search/conf-A.xml','./faceted_search/conf-B.xml' ]

    serverHandle = test_lib.startServer(args)
    if serverHandle == None:
        return -1

    #parse used to extract facet fields from input
    facet_parser= argparse.ArgumentParser()
    facet_parser.add_argument('-f',  metavar='facet', nargs='+', 
                                                    action='append')
    #construct the query
    failCount = 0
    for line in f_in:
        #get the query keyword and results
        value=line.split('||')
        tmpQueryValue= value[0].split(',')
        queryValue= tmpQueryValue[0].split()
        #line input is the format query, facet args||numResults
        #extracting and parsing the facet args
        facet_args=facet_parser.parse_args(tmpQueryValue[1].split())
        facetedFields= facet_args.f
        facet_args.f=[]
        resultValue=(value[1]).split()
        #construct the query
        query = prepareQuery(queryValue, facetedFields)
        

        # get facet correct result from file
        facetResultValue=[]
        for i in xrange(0, len(facetedFields)):
            facetResultValue.append(f_facet.next().strip())
           
        # do the query
        response_json = test_lib.searchRequest(query)
      
        #check the result
        failCount += checkResult(query, response_json, resultValue , facetResultValue )

    test_lib.killServer(serverHandle)
    print '=============================='
    return failCount

if __name__ == '__main__':    
   #Path of the query file
   #each line like "trust||01c90b4effb2353742080000" ---- query||record_ids(results)
   parser= argparse.ArgumentParser()
   parser.add_argument('--srch2', required=True,
                                                dest='binary_path')
   parser.add_argument('--qryNrslt', type=file, required=True,
                                                dest='queriesAndResults')
   parser.add_argument('--frslt', type=file, required=True,
                                                dest='facetResults')
#   parser.add_argument('-f',  metavar='facet', nargs='+', 
#                                                    action='append')

   args= parser.parse_args()
   binary_path = args.binary_path
   queriesAndResultsPath = args.queriesAndResults
   facetResultsPath = args.facetResults
   exitCode = testFacetedSearch(queriesAndResultsPath, facetResultsPath, binary_path)
   os._exit(exitCode)
