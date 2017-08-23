

#include "neighbor.h"

#include "ns3/log.h"
#include <algorithm>


namespace ns3
{

NS_LOG_COMPONENT_DEFINE ("MbrNeighbors");

namespace mbr
{

Neighbors::Neighbors (Time delay) :
  m_ntimer (Timer::CANCEL_ON_DESTROY)
{
  m_ntimer.SetDelay (delay);
  m_ntimer.SetFunction (&Neighbors::Purge, this);
//  m_txErrorCallback = MakeCallback (&Neighbors::ProcessTxError, this);
}

bool
Neighbors::IsNeighbor (Ipv4Address addr)
{
  Purge ();
  for (std::vector<Neighbor>::const_iterator i = m_nb.begin ();
       i != m_nb.end (); ++i)
    {
      if (i->m_ipAddress == addr)
        return true;
    }
  return false;
}

Time
Neighbors::GetExpireTime (Ipv4Address addr)
{
  Purge ();
  for (std::vector<Neighbor>::const_iterator i = m_nb.begin (); i
       != m_nb.end (); ++i)
    {
      if (i->m_ipAddress == addr)
        return (i->m_expireTime - Simulator::Now ());
    }
  return Seconds (0);
}

void
Neighbors::Update (Ipv4Address addr, Time expire, const uint8_t *mac, uint64_t geohash, uint16_t direction, double x, double y)
{
	Mac48Address tempmac;
	tempmac.CopyFrom(mac);
  for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
    if (i->m_ipAddress == addr)
      {
        i->m_expireTime
          = std::max (expire + Simulator::Now (), i->m_expireTime);
        if (i->m_hardwareAddress == Mac48Address ()) {

          i->m_hardwareAddress = tempmac;
        }
        i->m_x = x;
        i->m_y = y;
        return;
      }

  NS_LOG_LOGIC ("Open link to " << addr);
  Neighbor neighbor (addr, tempmac, expire + Simulator::Now (), geohash, direction, x, y);
  m_nb.push_back (neighbor);
  Purge ();
}

struct CloseNeighbor
{
  bool operator() (const Neighbors::Neighbor & nb) const
  {
    return ((nb.m_expireTime < Simulator::Now ()) || nb.close);
  }
};

void
Neighbors::Purge ()
{
  if (m_nb.empty ())
    return;

  CloseNeighbor pred;

  m_nb.erase (std::remove_if (m_nb.begin (), m_nb.end (), pred), m_nb.end ());
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}

void
Neighbors::ScheduleTimer ()
{
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}

/**
 * Search node's geohash from it's mac addr.
 * return
 *
 */
uint64_t Neighbors::GetGeohashFromMacInNb(uint8_t* mac, double *x, double *y)
{
	Mac48Address tempmac;
	tempmac.CopyFrom(mac);
	for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
		if (i->m_hardwareAddress == tempmac) {
			*x = i->m_x;
			*y = i->m_y;

			return i->m_geohash;

		}

	return 0;
}

/**
 * Search node's geohash from it's mac addr.
 * return
 *
 */
uint64_t Neighbors::GetGeohashFromIpInNb(Ipv4Address ip, uint8_t* to_mac, double *x, double *y)
{
	for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
		if (i->m_ipAddress == ip)
		{
			*x = i->m_x;
			*y = i->m_y;
			i->m_hardwareAddress.CopyTo(to_mac);
			return i->m_geohash;
		}
	return 0;
}

/**
 * get a random node which is in the range of the geohash set.
 * return 0 if unmatched.
 */
int Neighbors::GetnbFromsetRandom(Mac48Address *mac, GeoHashSetCoordinate *geohashset)
{
	int j,k;

	if(geohashset->sx != 3 && geohashset->sy != 3) {
//		mbr_dbg(debug_level, ANY, "neighbor_getnode_fromset_random: does not support sx: %d, sy: %d!\n",
//				geohashset->sx, geohashset->sy);
		return 0;
	}
	for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
		for(j = 0; j < geohashset->sy; j++) {
			for(k=0; k < geohashset->sx; k++) {
				if(i->m_geohash == geohashset->geohashset[j][k]) {
					*mac = i->m_hardwareAddress;
					return 1;
				}
			}
		}

	return 0;
}

int Neighbors::GetDtime(uint64_t geohash, uint16_t direct, GeoHashSetCoordinate *geohashset)
{
	int i, j;

	if(geohashset->sx != 3 && geohashset->sy != 3) {
//		mbr_dbg(debug_level, ANY, "get_dtime: does not support sx: %d, sy: %d!\n",
//				geohashset->sx, geohashset->sy);
		return -1;
	}

	for(i = 0; i < geohashset->sy; i++) {
		for(j=0; j < geohashset->sx; j++) {
			if(geohash == geohashset->geohashset[i][j])
			{
				if(45 <= direct && direct < 135) { //East
					return geohashset->sx - j;
					}
				if(225 <= direct && direct < 315) { //West
					return j + 1;
					}
				if(135 <= direct && direct < 225) { //South
					return geohashset->sy - i;
					}
				if(315 <= direct || direct < 45) { //North
					return i + 1;
					}
			}
		}
	}
	return -1;
}

/**
 * get the best node which is in the range of the geohash set based on the "relay zone dwelling time".
 * return 0 if unmatched.
 */
int Neighbors::GetnbFromsetBest(Mac48Address *ret_mac, GeoHashSetCoordinate *geohashset)
{
//	unsigned int i;
	Mac48Address best, temp;
	int dtime_best = 0;
	int dtime_curr = 0;
	int dtime_max = (geohashset->sx > geohashset->sy) ? geohashset->sx : geohashset->sy;

	if(geohashset->sx != 3 && geohashset->sy != 3) {
//		mbr_dbg(debug_level, ANY, "neighbor_getnode_fromset_best: does not support sx: %d, sy: %d!\n",
//				geohashset->sx, geohashset->sy);
		return 0;
	}

	for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i) {
		dtime_curr = GetDtime(i->m_geohash, i->m_direction, geohashset);
		if ( dtime_curr == dtime_max ) {
			*ret_mac = i->m_hardwareAddress;
			return 1;
		}
		else if(dtime_curr > dtime_best) {
			dtime_best = dtime_curr;
			best = i->m_hardwareAddress;
		}
	}

	if(dtime_best > 0)	{
		*ret_mac = best;
		return 1;
	}
	else
		return 0;
}
}
}

