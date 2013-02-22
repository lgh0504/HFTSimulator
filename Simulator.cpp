#include <fstream>
#include <iostream>
#include <sstream>

#include "Agent.h"
#include "Market.h"
#include "Order.h"
#include "OrderBook.h"
#include "DistributionUniform.h"
#include "DistributionGaussian.h"
#include "DistributionExponential.h"
#include "DistributionConstant.h"
#include "LiquidityProvider.h"
#include "NoiseTrader.h"
#include "Exceptions.h"
#include "Plot.h"
#include "Stats.h"
#include "RandomNumberGenerator.h"

#include <boost\thread.hpp>
#include "MarketMaker.h"

extern bool USE_PRIORITY = true;

//int nbAssets = cf.Value("mainParameters","nbAssets");

void plotOrderBook(Market *aMarket,Plot* aplotter,int a_orderBookId)
{
	std::vector<int> price;
	std::vector<int> priceQ;
	std::vector<int> priceMM;
	std::vector<int> priceQMM;
	int last;
	aMarket->getOrderBook(a_orderBookId)->getOrderBookForPlot(price,priceQ,priceMM,priceQMM);
	last = aMarket->getOrderBook(a_orderBookId)->getPrice();

	concurrency::concurrent_vector<int> historicPrices = aMarket->getOrderBook(a_orderBookId)->getHistoricPrices();
	concurrency::concurrent_vector<double> transactionsTimes = aMarket->getOrderBook(a_orderBookId)->getTransactionsTimes();

	int sizePrices = historicPrices.size();
	int sizeTransactionsTimes = transactionsTimes.size();

	//std::cout<<sizePrices<<std::endl;
	for (int k=0;k<sizePrices;k++){
		//	int a;
		//std::cout<< aMarket->getOrderBook(1)->getHistoricPrices()[k]<<std::endl;
		//std::cin>>a;
	}
	double variance=0;
	if (sizePrices>1){

		for (int z=0;z<(sizePrices-1);z++){
			//std::cout<<historicPrices[z+1]<<std::endl;
			//std::cout<<historicPrices[z]<<std::endl;
			//int a;
			//std::cin>>a;
			variance +=  pow(double( double( double(historicPrices[z+1])-double(historicPrices[z]))/double(historicPrices[z])),2) ;
		}
	}
//	std::cout<<"Transaction Time = "<<transactionsTimes[sizeTransactionsTimes-1]/100.0<<std::endl;
	if (transactionsTimes[sizeTransactionsTimes-1]!=0){
		double annualTime = ((((transactionsTimes[sizeTransactionsTimes-1]/1000.0)/60)/60)/24)/365; 
		variance = aMarket->getOrderBook(a_orderBookId)->getReturnsSumSquared()/annualTime;
	}
	double volatility = pow(variance, 0.5);

//	std::cout<<"vol = "<<volatility<<std::endl;
	aplotter->plotOrderBook(price,priceQ,last, volatility, priceMM, priceQMM);
}

int nbAssets = 1;
int storedDepth = 1;

// Parameters for the liquidity provider
double meanActionTimeLP = 0.35 ;
int meanVolumeLP = 100;
int meanPriceLagLP = 6;
double buyFrequencyLP = 0.25;
double cancelBuyFrequencyLP = 0.25;
double cancelSellFrequencyLP = 0.25;
double uniformCancellationProbability = 0.01;


//Parameters for the Market Maker
double buyFrequencyMM=1;
double cancelBuyFrequencyMM=0;
double cancelSellFrequencyMM=0;
double uniformCancellationProbabilityMM=0;

// Parameters for the liquidity takers : NT and LOT
double meanDeltaTimeMarketOrder = 2.2 ;
double percentageLargeOrders = 0.01 ;

double meanActionTimeNT = meanDeltaTimeMarketOrder / (1.0-percentageLargeOrders) ;
int meanVolumeNT = 100;
double buyFrequencyNT = 0.5;

//if you use a uniform distribution
int minVolumeNT=100;
int maxVolumeNT=300;

double meanActionTimeLOT = meanDeltaTimeMarketOrder / percentageLargeOrders ;
int meanVolumeLOT = 1000;
double buyFrequencyLOT = 0.5;

int nInitialOrders = 300;
double simulationTimeStart = 0 ;
double simulationTimeStop = 1000 ;
double printIntervals = 10; //900 ;
double impactMeasureLength = 60 ;

bool activateHFTPriority = true;

Market *myMarket;

void runOrderBook(){
	myMarket->getOrderBook(1)->runOrderBook();
}

int main(int argc, char* argv[])
{
	Plot * plotter2 = new Plot() ;
	concurrency::concurrent_vector<double> transactionsTimes;
	concurrency::concurrent_vector<int> historicPrices ;

	Plot * plotter = new Plot() ;
	std::ostringstream oss_marketName ;
	oss_marketName << "LargeTrader_" << percentageLargeOrders ;
	myMarket = new Market(oss_marketName.str());
	myMarket->createAssets(nbAssets);

	myMarket->getOrderBook(1)->setStoreOrderBookHistory(true,storedDepth);
	myMarket->getOrderBook(1)->setStoreOrderHistory(true);
	myMarket->getOrderBook(1)->activateHFTPriority(activateHFTPriority);
	//myMarket->getOrderBook(1)->setPrintOrderBookHistory(true,storedDepth);
	RandomNumberGenerator * myRNG = new RandomNumberGenerator();

	// Create one Liquidity Provider
	DistributionExponential * LimitOrderActionTimeDistribution = new DistributionExponential(myRNG, meanActionTimeLP) ;
	//DistributionExponential *  LimitOrderOrderVolumeDistribution = new DistributionExponential(myRNG, meanVolumeLP) ;
	DistributionGaussian * LimitOrderOrderVolumeDistribution = new DistributionGaussian(myRNG, 0.7*100, sqrt(0.2*100 ));
	//DistributionConstant * LimitOrderOrderVolumeDistribution = new DistributionConstant(myRNG, meanVolumeLP) ;
	DistributionExponential * LimitOrderOrderPriceDistribution = new DistributionExponential(myRNG, meanPriceLagLP) ;
	LiquidityProvider * myLiquidityProvider = new LiquidityProvider
		(
		myMarket, 
		LimitOrderActionTimeDistribution,
		LimitOrderOrderVolumeDistribution,
		LimitOrderOrderPriceDistribution,
		buyFrequencyLP,
		1,
		cancelBuyFrequencyLP,
		cancelSellFrequencyLP,
		uniformCancellationProbability
		) ;
	myMarket->registerAgent(myLiquidityProvider);

	//creation du group de thread
	boost::thread_group actors;
	//boost::thread t(boost::bind(&OrderBook::runOrderBook, myMarket->getOrderBook(1)));

	//create the orderBookThread
	actors.create_thread(boost::bind(&runOrderBook));
	std::cout<<"started "<<std::endl;

	// Submit nInitialOrders limit orders to initialize order book
	for(int n=0; n<nInitialOrders; n++)
	{
		myLiquidityProvider->makeAction(1, 0.0);
	}

	// Create one Noise Trader
	DistributionExponential * NoiseTraderActionTimeDistribution = new DistributionExponential(myRNG, meanActionTimeNT) ;
	DistributionUniform * NoiseTraderOrderTypeDistribution = new DistributionUniform(myRNG) ;
	//DistributionExponential * NoiseTraderOrderVolumeDistribution = new DistributionExponential(myRNG, meanVolumeNT) ;
	DistributionUniform * NoiseTraderOrderVolumeDistribution = new DistributionUniform(myRNG, minVolumeNT, maxVolumeNT) ;
	//DistributionConstant * NoiseTraderOrderVolumeDistribution = new DistributionConstant(myRNG, meanVolumeNT) ;
	NoiseTrader * myNoiseTrader = new NoiseTrader(myMarket, 
		NoiseTraderActionTimeDistribution,
		NoiseTraderOrderTypeDistribution,
		NoiseTraderOrderVolumeDistribution,
		buyFrequencyNT,1) ;
	myMarket->registerAgent(myNoiseTrader);


	// Create one Market Maker
	DistributionExponential * MarketOrderActionTimeDistribution = new DistributionExponential(myRNG, meanActionTimeLP) ;
	//DistributionExponential *  LimitOrderOrderVolumeDistribution = new DistributionExponential(myRNG, meanVolumeLP) ;
	DistributionGaussian * MarketOrderOrderVolumeDistribution = new DistributionGaussian(myRNG, 0.4*100, sqrt(0.2*100 ));
	//DistributionConstant * LimitOrderOrderVolumeDistribution = new DistributionConstant(myRNG, meanVolumeLP) ;
	DistributionExponential * MarketOrderOrderPriceDistribution = new DistributionExponential(myRNG, meanPriceLagLP) ;
	MarketMaker * myMarketMaker = new MarketMaker
		(
		myMarket,
		MarketOrderActionTimeDistribution,
		MarketOrderOrderVolumeDistribution,
		MarketOrderOrderPriceDistribution,
		buyFrequencyMM,
		1,
		cancelBuyFrequencyMM,
		cancelSellFrequencyMM,
		uniformCancellationProbabilityMM,
		0.1,
		activateHFTPriority
		) ;
	myMarket->registerAgent(myMarketMaker);



	// Simulate market
	std::cout << "Simulation starts. " << std::endl ;
	double currentTime = simulationTimeStart ;
	int i=1;

	//std::string input;
	//std::cin >> input;

	std::cout 
		<< "Time 0 : [bid ; ask] = " 
		<< "[" << myMarket->getOrderBook(1)->getBidPrice()/100.0 << " ; "
		<< myMarket->getOrderBook(1)->getAskPrice()/100.0 << "]"
		<< "  [sizeBid ; sizeAsk] = " 
		<< "[" << myMarket->getOrderBook(1)->getTotalBidQuantity() << " ; "
		<< myMarket->getOrderBook(1)->getTotalAskQuantity() << "]"
		<< std::endl ;
	std::cout << "Order book initialized." << std::endl ;

	//std::cin >> input;

	std::vector<double>  MidPriceTimeseries ;
	try{
		while(currentTime<simulationTimeStop)
		{
			//	std::cout<<"go while"<<std::endl;
			// Get next time of action
			currentTime += myMarket->getNextActionTime() ;
			// Select next player

			int spread = myMarket->getOrderBook(1)->getAskPrice() - myMarket->getOrderBook(1)->getBidPrice();
			Agent * actingAgent;

			if (spread >= 2*myMarket->getOrderBook(1)->getTickSize()) {
				myMarketMaker->makeAction( myMarketMaker->getTargetedStock(), currentTime) ;			
			}else if(myMarket->getOrderBook(1)->getTotalAskQuantity() < 2000 || myMarket->getOrderBook(1)->getTotalBidQuantity() < 2000){
				myLiquidityProvider->makeAction( myLiquidityProvider->getTargetedStock(), currentTime);
			}else{
				actingAgent = myMarket->getNextActor() ;
				actingAgent->makeAction( actingAgent->getTargetedStock(), currentTime);
			}
			if(currentTime>i*printIntervals)
			{
				std::cout
					<<"time: "<<currentTime << std::endl
					<< "[bid;ask]=" 
					<< "[" << myMarket->getOrderBook(1)->getBidPrice()/100.0 << " ; "
					<< myMarket->getOrderBook(1)->getAskPrice()/100.0 << "]"<< std::endl
					<< "[sizeBid;sizeAsk]=" 
					<< "[" << myMarket->getOrderBook(1)->getTotalBidQuantity()<<";" <<
					+ myMarket->getOrderBook(1)->getTotalAskQuantity() << "]"
					<< std::endl
					<< "[pendingLP ; pendingNT ; pendingMM]="
					<< "[" << myLiquidityProvider->getPendingOrders()->size()<<";"
					<< myNoiseTrader->getPendingOrders()->size() << ";"
					<< myMarketMaker->getPendingOrders()->size() << "]"
					<< std::endl;

				// Plot order book
				plotOrderBook(myMarket,plotter,1);

				// Agents'portfolios
				std::cout << "LP: nStock=\t" << myLiquidityProvider->getStockQuantity(1) 
					<< "\t Cash=\t" << myLiquidityProvider->getNetCashPosition() << std::endl ;				
				std::cout << "NT: nStock=\t" << myNoiseTrader->getStockQuantity(1) 
					<< "\t Cash=\t" << myNoiseTrader->getNetCashPosition() << std::endl;
				std::cout << "MM: nStock=\t" << myMarketMaker->getStockQuantity(1) 
					<< "\t Cash=\t" << myMarketMaker->getNetCashPosition() << std::endl<< std::endl<< std::endl ;				


				i++;
			}
			// Update clock
			myMarket->setNextActionTime() ;
		}
	}
	catch(Exception &e)
	{
		std::cout <<e.what()<< std::endl ;
	}


	int nbLPStocks = myLiquidityProvider->getStockQuantity(1);
	int nbMMStocks = myMarketMaker->getStockQuantity(1);
	double cashLP =  myLiquidityProvider->getNetCashPosition();
	double cashMM =  myMarketMaker->getNetCashPosition();
	if (nbLPStocks>0){
		cashLP += nbLPStocks * myMarket->getOrderBook(1)->getAskPrice();
	}
	else if (nbLPStocks<0){
		cashLP -= nbLPStocks * myMarket->getOrderBook(1)->getBidPrice();
	}
	if (nbMMStocks>0){
		cashMM += nbMMStocks * myMarket->getOrderBook(1)->getAskPrice();
	}
	else if (nbMMStocks<0){
		cashMM -= nbMMStocks * myMarket->getOrderBook(1)->getBidPrice();
	}
	std::cout<<"CASH POSITIONS : "<<std::endl;
	std::cout << "LP: CASH =\t" << cashLP/100.0<<std::endl;
	std::cout << "MM: CASH =\t" << cashMM/100.0<<std::endl;
	std::cout << "NT: CASH =\t" << myNoiseTrader->getNetCashPosition()/100.0 <<std::endl;

	historicPrices = myMarket->getOrderBook(1)->getHistoricPrices();
	transactionsTimes = myMarket->getOrderBook(1)->getTransactionsTimes();
	plotter2->plotPrices(transactionsTimes,historicPrices);

	std::cout<<"done!!!" << std::endl;
	int b;
	std::cin>>b;

	return 0;
}
