// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

import "@openzeppelin/contracts/token/ERC721/extensions/ERC721Enumerable.sol";
import "@openzeppelin/contracts/security/ReentrancyGuard.sol";
import "@openzeppelin/contracts/utils/math/SafeMath.sol";
import "@openzeppelin/contracts/access/AccessControl.sol";

contract ArtGallery is ERC721Enumerable, AccessControl, ReentrancyGuard {
    using SafeMath for uint256;

    struct Artwork {
        string ipfsHash;
        address artist;
        uint256 price;
        uint256 editionSize;
        uint256 royaltiesPercentage; // Percentage of sales price paid to the artist
        uint256 soldCount;
        uint256 highestBid;
        address highestBidder;
        uint256 auctionEndTime;
        bool auctionEnded;
    }

    bytes32 public constant CURATOR_ROLE = keccak256("CURATOR_ROLE");

    uint256 private _tokenCounter;
    mapping(uint256 => Artwork) private _artworks;
    mapping(uint256 => mapping(address => uint256)) private _bids;

    event ArtworkCreated(uint256 indexed tokenId, address indexed artist, string ipfsHash, uint256 price, uint256 editionSize, uint256 royaltiesPercentage);
    event ArtworkPurchased(uint256 indexed tokenId, address indexed buyer, uint256 price);
    event ArtworkSoldOut(uint256 indexed tokenId);
    event BidPlaced(uint256 indexed tokenId, address indexed bidder, uint256 amount);
    event AuctionEnded(uint256 indexed tokenId, address indexed winner, uint256 amount);

    constructor() ERC721("Artwork", "ART") {
        _setupRole(DEFAULT_ADMIN_ROLE, msg.sender);
        _setupRole(CURATOR_ROLE, msg.sender);
    }

    modifier onlyCurator() {
        require(hasRole(CURATOR_ROLE, msg.sender), "Caller is not a curator");
        _;
    }

    function createArtwork(string memory ipfsHash, uint256 price, uint256 editionSize, uint256 royaltiesPercentage) external {
        require(editionSize > 0, "Edition size must be greater than 0");
        _tokenCounter++;
        uint256 tokenId = _tokenCounter;
        _mint(msg.sender, tokenId);
        _artworks[tokenId] = Artwork(ipfsHash, msg.sender, price, editionSize, royaltiesPercentage, 0, 0, address(0), 0, false);
        emit ArtworkCreated(tokenId, msg.sender, ipfsHash, price, editionSize, royaltiesPercentage);
    }

    function purchaseArtwork(uint256 tokenId) external payable nonReentrant {
        require(_exists(tokenId), "Token does not exist");
        Artwork storage artwork = _artworks[tokenId];
        require(!artwork.auctionEnded, "Auction is ongoing");
        require(artwork.soldCount < artwork.editionSize, "Artwork sold out");
        require(msg.value >= artwork.price, "Insufficient payment");
        address payable artist = payable(artwork.artist);
        uint256 royalties = (msg.value * artwork.royaltiesPercentage) / 100;
        artist.transfer(msg.value - royalties);
        payable(owner()).transfer(royalties);
        _transfer(artist, msg.sender, tokenId);
        artwork.soldCount++;
        emit ArtworkPurchased(tokenId, msg.sender, msg.value);
        if (artwork.soldCount == artwork.editionSize) {
            emit ArtworkSoldOut(tokenId);
        }
    }

    function placeBid(uint256 tokenId) external payable nonReentrant {
        require(_exists(tokenId), "Token does not exist");
        Artwork storage artwork = _artworks[tokenId];
        require(!artwork.auctionEnded, "Auction has ended");
        require(msg.value > artwork.highestBid, "Bid must be higher than current highest bid");
        require(msg.sender != artwork.artist, "Artist cannot bid on their own artwork");
        if (artwork.highestBidder != address(0)) {
            payable(artwork.highestBidder).transfer(artwork.highestBid);
        }
        artwork.highestBid = msg.value;
        artwork.highestBidder = msg.sender;
        _bids[tokenId][msg.sender] = msg.value;
        emit BidPlaced(tokenId, msg.sender, msg.value);
    }

    function endAuction(uint256 tokenId) external onlyCurator {
        require(_exists(tokenId), "Token does not exist");
        Artwork storage artwork = _artworks[tokenId];
        require(!artwork.auctionEnded, "Auction has already ended");
        require(block.timestamp >= artwork.auctionEndTime, "Auction has not ended yet");
        artwork.auctionEnded = true;
        if (artwork.highestBidder != address(0)) {
            address payable artist = payable(artwork.artist);
            artist.transfer(artwork.highestBid);
            _transfer(artist, artwork.highestBidder, tokenId);
            emit AuctionEnded(tokenId, artwork.highestBidder, artwork.highestBid);
        }
    }

    function getArtwork(uint256 tokenId) external view returns (string memory ipfsHash, address artist, uint256 price, uint256 editionSize, uint256 royaltiesPercentage, uint256 soldCount, uint256 highestBid, address highestBidder, uint256 auctionEndTime, bool auctionEnded) {
        require(_exists(tokenId), "Token does not exist");
        Artwork storage artwork = _artworks[tokenId];
        return (artwork.ipfsHash, artwork.artist, artwork.price, artwork.editionSize, artwork.royaltiesPercentage, artwork.soldCount, artwork.highestBid, artwork.highestBidder, artwork.auctionEndTime, artwork.auctionEnded);
    }

    function grantCuratorRole(address curator) external onlyRole(DEFAULT_ADMIN_ROLE) {
        grantRole(CURATOR_ROLE, curator);
    }

    function revokeCuratorRole(address curator) external onlyRole(DEFAULT_ADMIN_ROLE) {
        revokeRole(CURATOR_ROLE, curator);
    }

    function isCurator(address account) external view returns (bool) {
        return hasRole(CURATOR_ROLE, account);
    }

    function totalArtworks() external view returns (uint256) {
        return _tokenCounter;
    }

    function artworksOf(address owner) external view returns (uint256[] memory) {
        uint256 tokenCount = balanceOf(owner);
        uint256[] memory tokens = new uint256[](tokenCount);
        for (uint256 i = 0; i < tokenCount; i++) {
            tokens[i] = tokenOfOwnerByIndex(owner, i);
        }
        return tokens;
    }

    function getBid(uint256 tokenId, address bidder) external view returns (uint256) {
        return _bids[tokenId][bidder];
    }
}
