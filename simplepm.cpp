#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <queue>

const double PI = 3.14159265358979323846;
const double INF = 1e20;
const double EPS = 1e-6;
const double MaxDepth = 5;

// *** ���̑��̊֐� ***
inline double rand01() { return (double)rand()/RAND_MAX; }

// *** �f�[�^�\�� ***
struct Vec {
	double x, y, z;
	Vec(const double x_ = 0, const double y_ = 0, const double z_ = 0) : x(x_), y(y_), z(z_) {}
	inline Vec operator+(const Vec &b) const {return Vec(x + b.x, y + b.y, z + b.z);}
	inline Vec operator-(const Vec &b) const {return Vec(x - b.x, y - b.y, z - b.z);}
	inline Vec operator*(const double b) const {return Vec(x * b, y * b, z * b);}
	inline Vec operator/(const double b) const {return Vec(x / b, y / b, z / b);}
	inline const double LengthSquared() const { return x*x + y*y + z*z; }
	inline const double Length() const { return sqrt(LengthSquared()); }
};
inline Vec operator*(double f, const Vec &v) { return v * f; }
inline Vec Normalize(const Vec &v) { return v / v.Length(); }
// �v�f���Ƃ̐ς��Ƃ�
inline const Vec Multiply(const Vec &v1, const Vec &v2) {
	return Vec(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
}
inline const double Dot(const Vec &v1, const Vec &v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}
inline const Vec Cross(const Vec &v1, const Vec &v2) {
	return Vec((v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z), (v1.x * v2.y) - (v1.y * v2.x));
}
typedef Vec Color;
const Color BackgroundColor(0.0, 0.0, 0.0);

struct Ray {
	Vec org, dir;
	Ray(const Vec org_, const Vec &dir_) : org(org_), dir(dir_) {}
};

enum ReflectionType {
	DIFFUSE,    // ���S�g�U�ʁB������Lambertian�ʁB
	SPECULAR,   // ���z�I�ȋ��ʁB
	REFRACTION, // ���z�I�ȃK���X�I�����B
};

struct Sphere {
	double radius;
	Vec position;
	Color emission, color;
	ReflectionType ref_type;

	Sphere(const double radius_, const Vec &position_, const Color &emission_, const Color &color_, const ReflectionType ref_type_) :
	  radius(radius_), position(position_), emission(emission_), color(color_), ref_type(ref_type_) {}
	// ���͂�ray�ɑ΂�������_�܂ł̋�����Ԃ��B�������Ȃ�������0��Ԃ��B
	const double intersect(const Ray &ray) {
		Vec o_p = position - ray.org;
		const double b = Dot(o_p, ray.dir), det = b * b - Dot(o_p, o_p) + radius * radius;
		if (det >= 0.0) {
			const double sqrt_det = sqrt(det);
			const double t1 = b - sqrt_det, t2 = b + sqrt_det;
			if (t1 > EPS)		return t1;
			else if(t2 > EPS)	return t2;
		}
		return 0.0;
	}
};

// �ȉ��t�H�g���}�b�v�p�f�[�^�\��
struct Photon {
	Vec position;
	Color power;
	Vec incident;

	Photon(const Vec& position_, const Color& power_, const Vec& incident_) :
	position(position_), power(power_), incident(incident_) {}
};

// PhotonQueue�ɏ悹�邽�߂̃f�[�^�\���Bstruct Photon�Ƃ͕ʂɗp�ӂ���B
struct PhotonForQueue {
	const Photon *photon;
	double distance2;
	PhotonForQueue(const Photon *photon_, const double distance2_) : photon(photon_), distance2(distance2_) {}
	bool operator<(const PhotonForQueue &b) const {
		return distance2 < b.distance2;
	}
};
// k-NN search�Ŏg���L���[
typedef std::priority_queue<PhotonForQueue, std::vector<PhotonForQueue>> PhotonQueue;

// �t�H�g�����i�[���邽�߂�KD-tree
class PhotonMap {
public:
	// k-NN search�̃N�G��
	struct Query {
		double max_distance2; // �T���̍ő唼�a
		size_t max_photon_num; // �ő�t�H�g����
		Vec search_position; // �T�����S
		Vec normal; // �T�����S�ɂ�����@��
		Query(const Vec &search_position_, const Vec &normal_, const double max_distance2_, const size_t max_photon_num_) :
		max_distance2(max_distance2_), normal(normal_), max_photon_num(max_photon_num_), search_position(search_position_) {}
	};
private:
	std::vector<Photon> photons;
	struct KDTreeNode {
		Photon* photon;
		KDTreeNode* left;
		KDTreeNode* right;
		int axis;
	};
	KDTreeNode* root;
	void delete_kdtree(KDTreeNode* node) {
		if (node == NULL)
			return;
		delete_kdtree(node->left);
		delete_kdtree(node->right);
		delete node;
	}

	// �t�c�[��k-NN search�B
	void locate_photons(PhotonQueue* pqueue, KDTreeNode* node, PhotonMap::Query &query) {
		if (node == NULL)
			return;
		const int axis = node->axis;

		double delta;
		switch (axis) {
		case 0: delta = query.search_position.x - node->photon->position.x; break;
		case 1: delta = query.search_position.y - node->photon->position.y; break;
		case 2: delta = query.search_position.z - node->photon->position.z; break;
		}

		// �t�H�g��<->�T�����S�̋������ݒ蔼�a�ȉ��@���@�t�H�g��<->�T�����S�̖@�������̋��������ȉ��@�Ƃ��������Ȃ炻�̃t�H�g�����i�[
		const Vec dir = node->photon->position - query.search_position;
		const double distance2 = dir.LengthSquared();
		const double dt = Dot(query.normal, dir / sqrt(distance2));
		if (distance2 < query.max_distance2 && fabs(dt) <= query.max_distance2 * 0.01) {
			pqueue->push(PhotonForQueue(node->photon, distance2));
			if (pqueue->size() > query.max_photon_num) {
				pqueue->pop();
				query.max_distance2 = pqueue->top().distance2;
			}
		}
		if (delta > 0.0) { // �݂�
			locate_photons(pqueue,node->right, query);
			if (delta * delta < query.max_distance2) {
				locate_photons(pqueue, node->left, query);
			}
		} else { // �Ђ���
			locate_photons(pqueue,node->left, query);
			if (delta * delta < query.max_distance2) {
				locate_photons(pqueue, node->right, query);
			}
		}

	}
	
	static bool kdtree_less_operator_x(const Photon& left, const Photon& right) {
		return left.position.x < right.position.x;
	}
	static bool kdtree_less_operator_y(const Photon& left, const Photon& right) {
		return left.position.y < right.position.y;
	}
	static bool kdtree_less_operator_z(const Photon& left, const Photon& right) {
		return left.position.z < right.position.z;
	}
	
	KDTreeNode* create_kdtree_sub(std::vector<Photon>::iterator begin, std::vector<Photon>::iterator end, int depth) {
		if (end - begin <= 0) {
			return NULL;
		}
		const int axis = depth % 3;
		// �����l
		switch (axis) {
		case 0: std::sort(begin, end, kdtree_less_operator_x); break;
		case 1: std::sort(begin, end, kdtree_less_operator_y); break;
		case 2: std::sort(begin, end, kdtree_less_operator_z); break;
		}
		const int median = (end - begin) / 2;
		KDTreeNode* node = new KDTreeNode;
		node->axis = axis;
		node->photon = &(*(begin + median));
		// �q��
		node->left  = create_kdtree_sub(begin,              begin + median, depth + 1);
		node->right = create_kdtree_sub(begin + median + 1, end,            depth + 1);
		return node;
	}
public:
	PhotonMap() {
		root = NULL;
	}
	virtual ~PhotonMap() {
		delete_kdtree(root);
	}
	size_t Size() {
		return photons.size();
	}
	void SearchKNN(PhotonQueue* pqueue, PhotonMap::Query &query) {
		locate_photons(pqueue, root, query);
	}
	void AddPhoton(const Photon &photon) {
		photons.push_back(photon);
	}
	void CreateKDtree() {
		root = create_kdtree_sub(photons.begin(), photons.end(), 0);
	}
};

// *** �����_�����O����V�[���f�[�^ ****
// from smallpt
Sphere spheres[] = {
	Sphere(5.0, Vec(50.0, 75.0, 81.6),Color(12,12,12), Color(), DIFFUSE),//�Ɩ�
	Sphere(1e5, Vec( 1e5+1,40.8,81.6), Color(), Color(0.75, 0.25, 0.25),DIFFUSE),// ��
	Sphere(1e5, Vec(-1e5+99,40.8,81.6),Color(), Color(0.25, 0.25, 0.75),DIFFUSE),// �E
	Sphere(1e5, Vec(50,40.8, 1e5),     Color(), Color(0.75, 0.75, 0.75),DIFFUSE),// ��
	Sphere(1e5, Vec(50,40.8,-1e5+170), Color(), Color(), DIFFUSE),// ��O
	Sphere(1e5, Vec(50, 1e5, 81.6),    Color(), Color(0.75, 0.75, 0.75),DIFFUSE),// ��
	Sphere(1e5, Vec(50,-1e5+81.6,81.6),Color(), Color(0.75, 0.75, 0.75),DIFFUSE),// �V��
	Sphere(16.5,Vec(27,16.5,47),       Color(), Color(1,1,1)*.99, SPECULAR),// ��
	Sphere(16.5,Vec(73,16.5,78),       Color(), Color(1,1,1)*.99, REFRACTION),//�K���X
};
const int LightID = 0;

// *** �����_�����O�p�֐� ***
// �V�[���Ƃ̌�������֐�
inline bool intersect_scene(const Ray &ray, double *t, int *id) {
	const double n = sizeof(spheres) / sizeof(Sphere);
	*t  = INF;
	*id = -1;
	for (int i = 0; i < int(n); i ++) {
		double d = spheres[i].intersect(ray);
		if (d > 0.0 && d < *t) {
			*t  = d;
			*id = i;
		}
	}
	return *t < INF;
}

// �t�H�g���ǐՖ@�ɂ��t�H�g���}�b�v�\�z
void create_photon_map(const int shoot_photon_num, PhotonMap *photon_map) {
	std::cout << "Shooting photons... (" << shoot_photon_num << " photons)" << std::endl;
	for (int i = 0; i < shoot_photon_num; i ++) {
		// ��������t�H�g���𔭎˂���
		// ������̈�_���T���v�����O����	
		const double r1 = 2 * PI * rand01();
		const double r2 = 1.0 - 2.0 * rand01() ;
		const Vec light_pos = spheres[LightID].position + ((spheres[LightID].radius + EPS) * Vec(sqrt(1.0 - r2*r2) * cos(r1), sqrt(1.0 - r2*r2) * sin(r1), r2));

		const Vec normal = Normalize(light_pos - spheres[LightID].position);
		// ������̓_���甼���T���v�����O����
		Vec w, u, v;
		w = normal;
		if (fabs(w.x) > 0.1)
			u = Normalize(Cross(Vec(0.0, 1.0, 0.0), w));
		else
			u = Normalize(Cross(Vec(1.0, 0.0, 0.0), w));
		v = Cross(w, u);
		// �R�T�C�����ɔ�Ⴓ����B�t�H�g�����^�Ԃ̂����ˋP�x�ł͂Ȃ����ˑ��ł��邽�߁B
		const double u1 = 2 * PI * rand01();
		const double u2 = rand01(), u2s = sqrt(u2);
		Vec light_dir = Normalize((u * cos(u1) * u2s + v * sin(u1) * u2s + w * sqrt(1.0 - u2)));

		Ray now_ray(light_pos, light_dir);
		// emission�̒l�͕��ˋP�x�����A�t�H�g�����^�Ԃ͕̂��ˑ��Ȃ̂ŕϊ�����K�v������B
		// L�i���ˋP�x�j= d��/(cos��d��dA)�Ȃ̂ŁA�����̕��ˑ��̓� = ���L�Ecos��d��dA�ɂȂ�B����͋������Ŋ��S�g�U�����ł��邱�Ƃ���
		// ����̔C�ӂ̏ꏊ�A�C�ӂ̕����ɓ��������ˋP�xLe�����B�i���ꂪemission�̒l�j����āA
		// �� = Le�E���cos��d��dA�ŁALe�E��dA��cos��d�ւƂȂ�A��dA�͋��̖ʐςȂ̂�4��r^2�A��cos��d�ւ͗��̊p�̐ϕ��Ȃ̂Ń΂ƂȂ�B
		// ����āA�� = Le�E4��r^2�E�΂ƂȂ�B���̒l���������甭�˂���t�H�g�����Ŋ����Ă��Έ�̃t�H�g�����^�ԕ��ˑ������܂�B
		Color now_flux = spheres[LightID].emission * 4.0 * PI * pow(spheres[LightID].radius, 2.0) * PI / shoot_photon_num;

		// �t�H�g�����V�[������
		bool trace_end = false;
		for (;!trace_end;) {
			// ���ˑ���0.0�ȃt�H�g����ǐՂ��Ă����傤���Ȃ��̂őł��؂�
			if (std::max(now_flux.x, std::max(now_flux.y, now_flux.z)) <= 0.0)
				break;

			double t; // ���C����V�[���̌����ʒu�܂ł̋���
			int id;   // ���������V�[�����I�u�W�F�N�g��ID
			if (!intersect_scene(now_ray, &t, &id))
				break;
			const Sphere &obj = spheres[id];
			const Vec hitpoint = now_ray.org + t * now_ray.dir; // �����ʒu
			const Vec normal  = Normalize(hitpoint - obj.position); // �����ʒu�̖@��
			const Vec orienting_normal = Dot(normal, now_ray.dir) < 0.0 ? normal : (-1.0 * normal); // �����ʒu�̖@���i���̂���̃��C�̓��o���l���j

			switch (obj.ref_type) {
			case DIFFUSE: {
				// �g�U�ʂȂ̂Ńt�H�g�����t�H�g���}�b�v�Ɋi�[����
				photon_map->AddPhoton(Photon(hitpoint, now_flux, now_ray.dir));

				// ���˂��邩�ǂ��������V�A�����[���b�g�Ō��߂�
				// ��ɂ���Ċm���͔C�ӁB����̓t�H�g���}�b�v�{�ɏ]����RGB�̔��˗��̕��ς��g��
				const double probability = (obj.color.x + obj.color.y + obj.color.z) / 3;
				if (probability > rand01()) { // ����
					// orienting_normal�̕�������Ƃ������K�������(w, u, v)�����B���̊��ɑ΂��锼�����Ŏ��̃��C���΂��B
					Vec w, u, v;
					w = orienting_normal;
					if (fabs(w.x) > 0.1)
						u = Normalize(Cross(Vec(0.0, 1.0, 0.0), w));
					else
						u = Normalize(Cross(Vec(1.0, 0.0, 0.0), w));
					v = Cross(w, u);
					// �R�T�C�������g�����d�_�I�T���v�����O
					const double r1 = 2 * PI * rand01();
					const double r2 = rand01(), r2s = sqrt(r2);
					Vec dir = Normalize((u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1.0 - r2)));
					
					now_ray = Ray(hitpoint, dir);
					now_flux = Multiply(now_flux, obj.color) / probability;
					continue;
				} else { // �z���i���Ȃ킿�����ŒǐՏI���j
					trace_end = true;
					continue;
				}
			} break;
			case SPECULAR: {
				// ���S���ʂȂ̂Ńt�H�g���i�[���Ȃ�
				// ���S���ʂȂ̂Ń��C�̔��˕����͌���I�B
				now_ray = Ray(hitpoint, now_ray.dir - normal * 2.0 * Dot(normal, now_ray.dir));
				now_flux = Multiply(now_flux, obj.color);
				continue;
			} break;
			case REFRACTION: {
				// ��͂�t�H�g���i�[���Ȃ�
				Ray reflection_ray = Ray(hitpoint, now_ray.dir - normal * 2.0 * Dot(normal, now_ray.dir));
				bool into = Dot(normal, orienting_normal) > 0.0; // ���C���I�u�W�F�N�g����o��̂��A����̂�

				// Snell�̖@��
				const double nc = 1.0; // �^��̋��ܗ�
				const double nt = 1.5; // �I�u�W�F�N�g�̋��ܗ�
				const double nnt = into ? nc / nt : nt / nc;
				const double ddn = Dot(now_ray.dir, orienting_normal);
				const double cos2t = 1.0 - nnt * nnt * (1.0 - ddn * ddn);
		
				if (cos2t < 0.0) { // �S���˂���
					now_ray = reflection_ray;
					now_flux = Multiply(now_flux, obj.color);
					continue;
				}
				// ���܂��Ă�������
				Vec tdir = Normalize(now_ray.dir * nnt - normal * (into ? 1.0 : -1.0) * (ddn * nnt + sqrt(cos2t)));
				const double probability  = 0.5;

				// ���܂Ɣ��˂̂ǂ��炩�����ǐՂ���B
				// ���V�A�����[���b�g�Ō��肷��B
				if (rand01() < probability) { // ����
					now_ray = Ray(hitpoint, tdir);
					now_flux = Multiply(now_flux, obj.color);
					continue;
				} else { // ����
					now_ray = reflection_ray;
					now_flux = Multiply(now_flux, obj.color);
					continue;
				}
			} break;
			}
		}
	}
	std::cout << "Done. (" << photon_map->Size() <<  " photons are stored)" << std::endl;
	std::cout << "Creating KD-tree..." << std::endl;
	photon_map->CreateKDtree();
	std::cout << "Done." << std::endl;
}

// ray��������̕��ˋP�x�����߂�
Color radiance(const Ray &ray, const int depth, PhotonMap *photon_map, const double gather_radius, const int gahter_max_photon_num) {
	double t; // ���C����V�[���̌����ʒu�܂ł̋���
	int id;   // ���������V�[�����I�u�W�F�N�g��ID
	if (!intersect_scene(ray, &t, &id))
		return BackgroundColor;

	const Sphere &obj = spheres[id];
	const Vec hitpoint = ray.org + t * ray.dir; // �����ʒu
	const Vec normal  = Normalize(hitpoint - obj.position); // �����ʒu�̖@��
	const Vec orienting_normal = Dot(normal, ray.dir) < 0.0 ? normal : (-1.0 * normal); // �����ʒu�̖@���i���̂���̃��C�̓��o���l���j

	// �F�̔��˗��ő�̂��̂𓾂�B���V�A�����[���b�g�Ŏg���B
	// ���V�A�����[���b�g��臒l�͔C�ӂ����F�̔��˗������g���Ƃ��ǂ��B
	double russian_roulette_probability = std::max(obj.color.x, std::max(obj.color.y, obj.color.z));
	// ���ȏヌ�C��ǐՂ����烍�V�A�����[���b�g�����s���ǐՂ�ł��؂邩�ǂ����𔻒f����
	if (depth > MaxDepth) {
		if (rand01() >= russian_roulette_probability)
			return obj.emission;
	} else
		russian_roulette_probability = 1.0; // ���V�A�����[���b�g���s���Ȃ�����

	switch (obj.ref_type) {
	case DIFFUSE: {
		// �t�H�g���}�b�v�������ĕ��ˋP�x���肷��
		PhotonQueue pqueue;
		// k�ߖT�T���Bgather_radius���a���̃t�H�g�����ő�gather_max_photon_num�W�߂Ă���
		photon_map->SearchKNN(&pqueue, PhotonMap::Query(hitpoint, orienting_normal, gather_radius, gahter_max_photon_num));
		Color accumulated_flux;
		double max_distance2 = -1;

		// �L���[����t�H�g�������o��vector�Ɋi�[����
		std::vector<const PhotonForQueue> photons;
		photons.reserve(pqueue.size());
		for (;!pqueue.empty();) {
			PhotonForQueue p = pqueue.top(); pqueue.pop();
			photons.push_back(p);
			max_distance2 = std::max(max_distance2, p.distance2);
		}

		// �~���t�B���^���g�p���ĕ��ˋP�x���肷��
		const double max_distance = sqrt(max_distance2);
		const double k = 1.1;
		for (int i = 0; i < photons.size(); i ++) {
			const double w = 1.0 - (sqrt(photons[i].distance2) / (k * max_distance)); // �~���t�B���^�̏d��
			const Color v = Multiply(obj.color, photons[i].photon->power) / PI; // Diffuse�ʂ�BRDF = 1.0 / �΂ł������̂ł����������
			accumulated_flux = accumulated_flux + w * v;
		}
		accumulated_flux = accumulated_flux / (1.0 - 2.0 / (3.0 * k)); // �~���t�B���^�̌W��
		if (max_distance2 > 0.0) {
			return obj.emission + accumulated_flux / (PI * max_distance2) / russian_roulette_probability;
		}
	} break;


	// SPECULAR��REFRACTION�̏ꍇ�̓p�X�g���[�V���O�ƂقƂ�Ǖς��Ȃ��B
	// �P���ɔ��˕�������ܕ����̕��ˋP�x(Radiance)��radiance()�ŋ��߂邾���B

	case SPECULAR: {
		// ���S���ʂɃq�b�g�����ꍇ�A���˕���������ˋP�x��������Ă���
		return obj.emission + radiance(Ray(hitpoint, ray.dir - normal * 2.0 * Dot(normal, ray.dir)), depth+1, photon_map, gather_radius, gahter_max_photon_num);
	} break;
	case REFRACTION: {
		Ray reflection_ray = Ray(hitpoint, ray.dir - normal * 2.0 * Dot(normal, ray.dir));
		bool into = Dot(normal, orienting_normal) > 0.0; // ���C���I�u�W�F�N�g����o��̂��A����̂�

		// Snell�̖@��
		const double nc = 1.0; // �^��̋��ܗ�
		const double nt = 1.5; // �I�u�W�F�N�g�̋��ܗ�
		const double nnt = into ? nc / nt : nt / nc;
		const double ddn = Dot(ray.dir, orienting_normal);
		const double cos2t = 1.0 - nnt * nnt * (1.0 - ddn * ddn);
		
		if (cos2t < 0.0) { // �S���˂���	
			// ���˕���������ˋP�x��������Ă���
			return obj.emission + Multiply(obj.color,
				radiance(Ray(hitpoint, ray.dir - normal * 2.0 * Dot(normal, ray.dir)), depth+1, photon_map, gather_radius, gahter_max_photon_num)) / russian_roulette_probability;
		}
		// ���܂��Ă�������
		Vec tdir = Normalize(ray.dir * nnt - normal * (into ? 1.0 : -1.0) * (ddn * nnt + sqrt(cos2t)));

		// Schlick�ɂ��Fresnel�̔��ˌW���̋ߎ�
		const double a = nt - nc, b = nt + nc;
		const double R0 = (a * a) / (b * b);
		const double c = 1.0 - (into ? -ddn : Dot(tdir, normal));
		const double Re = R0 + (1.0 - R0) * pow(c, 5.0);
		const double Tr = 1.0 - Re; // ���܌��̉^�Ԍ��̗�
		const double probability  = 0.25 + 0.5 * Re;
		
		// ���ȏヌ�C��ǐՂ�������܂Ɣ��˂̂ǂ��炩�����ǐՂ���B�i�����Ȃ��Ǝw���I�Ƀ��C��������j
		// ���V�A�����[���b�g�Ō��肷��B
		if (depth > 2) {
			if (rand01() < probability) { // ����
				return obj.emission +
					Multiply(obj.color, radiance(reflection_ray, depth+1, photon_map, gather_radius, gahter_max_photon_num) * Re)
					/ probability
					/ russian_roulette_probability;
			} else { // ����
				return obj.emission +
					Multiply(obj.color, radiance(Ray(hitpoint, tdir), depth+1, photon_map, gather_radius, gahter_max_photon_num) * Tr)
					/ (1.0 - probability)
					/ russian_roulette_probability;
			}
		} else { // ���܂Ɣ��˂̗�����ǐ�
			return obj.emission +
				Multiply(obj.color, radiance(reflection_ray, depth+1, photon_map, gather_radius, gahter_max_photon_num) * Re
				+ radiance(Ray(hitpoint, tdir), depth+1, photon_map, gather_radius, gahter_max_photon_num) * Tr) / russian_roulette_probability;
		}
	} break;
	}

	return Color();
}


// *** .hdr�t�H�[�}�b�g�ŏo�͂��邽�߂̊֐� ***
struct HDRPixel {
	unsigned char r, g, b, e;
	HDRPixel(const unsigned char r_ = 0, const unsigned char g_ = 0, const unsigned char b_ = 0, const unsigned char e_ = 0) :
	r(r_), g(g_), b(b_), e(e_) {};
	unsigned char get(int idx) {
		switch (idx) {
		case 0: return r;
		case 1: return g;
		case 2: return b;
		case 3: return e;
		} return 0;
	}

};

// double��RGB�v�f��.hdr�t�H�[�}�b�g�p�ɕϊ�
HDRPixel get_hdr_pixel(const Color &color) {
	double d = std::max(color.x, std::max(color.y, color.z));
	if (d <= 1e-32)
		return HDRPixel();
	int e;
	double m = frexp(d, &e); // d = m * 2^e
	d = m * 256.0 / d;
	return HDRPixel(color.x * d, color.y * d, color.z * d, e + 128);
}

// �����o���p�֐�
void save_hdr_file(const std::string &filename, const Color* image, const int width, const int height) {
	FILE *fp = fopen(filename.c_str(), "wb");
	if (fp == NULL) {
		std::cerr << "Error: " << filename << std::endl;
		return;
	}
	// .hdr�t�H�[�}�b�g�ɏ]���ăf�[�^����������
	// �w�b�_
	unsigned char ret = 0x0a;
	fprintf(fp, "#?RADIANCE%c", (unsigned char)ret);
	fprintf(fp, "# Made with 100%% pure HDR Shop%c", ret);
	fprintf(fp, "FORMAT=32-bit_rle_rgbe%c", ret);
	fprintf(fp, "EXPOSURE=1.0000000000000%c%c", ret, ret);

	// �P�x�l�����o��
	fprintf(fp, "-Y %d +X %d%c", height, width, ret);
	for (int i = height - 1; i >= 0; i --) {
		std::vector<HDRPixel> line;
		for (int j = 0; j < width; j ++) {
			HDRPixel p = get_hdr_pixel(image[j + i * width]);
			line.push_back(p);
		}
		fprintf(fp, "%c%c", 0x02, 0x02);
		fprintf(fp, "%c%c", (width >> 8) & 0xFF, width & 0xFF);
		for (int i = 0; i < 4; i ++) {
			for (int cursor = 0; cursor < width;) {
				const int cursor_move = std::min(127, width - cursor);
				fprintf(fp, "%c", cursor_move);
				for (int j = cursor;  j < cursor + cursor_move; j ++)
					fprintf(fp, "%c", line[j].get(i));
				cursor += cursor_move;
			}
		}
	}

	fclose(fp);
}

int main(int argc, char **argv) {
	int width = 640;
	int height = 480;
	int photon_num = 50000;
	double gather_photon_radius = 32.0;
	int gahter_max_photon_num = 64;

	// �J�����ʒu
	Ray camera(Vec(50.0, 52.0, 295.6), Normalize(Vec(0.0, -0.042612, -1.0)));
	// �V�[�����ł̃X�N���[����x,y�����̃x�N�g��
	Vec cx = Vec(width * 0.5135 / height);
	Vec cy = Normalize(Cross(cx, camera.dir)) * 0.5135;
	Color *image = new Color[width * height];

	// �t�H�g���}�b�v�\�z
	PhotonMap photon_map;
	create_photon_map(photon_num, &photon_map);
	
// #pragma omp parallel for schedule(dynamic, 1)
	for (int y = 0; y < height; y ++) {
		std::cerr << "Rendering " << (100.0 * y / (height - 1)) << "%" << std::endl;
		srand(y * y * y);
		for (int x = 0; x < width; x ++) {
			int image_index = y * width + x;	
			image[image_index] = Color();

			// 2x2�̃T�u�s�N�Z���T���v�����O
			for (int sy = 0; sy < 2; sy ++) {
				for (int sx = 0; sx < 2; sx ++) {
					// �e���g�t�B���^�[�ɂ���ăT���v�����O
					// �s�N�Z���͈͂ň�l�ɃT���v�����O����̂ł͂Ȃ��A�s�N�Z�������t�߂ɃT���v������������W�܂�悤�ɕ΂�𐶂�������
					const double r1 = 2.0 * rand01(), dx = r1 < 1.0 ? sqrt(r1) - 1.0 : 1.0 - sqrt(2.0 - r1);
					const double r2 = 2.0 * rand01(), dy = r2 < 1.0 ? sqrt(r2) - 1.0 : 1.0 - sqrt(2.0 - r2);
					Vec dir = cx * (((sx + 0.5 + dx) / 2.0 + x) / width - 0.5) +
								cy * (((sy + 0.5 + dy) / 2.0 + y) / height- 0.5) + camera.dir;
					image[image_index] = image[image_index] + radiance(Ray(camera.org + dir * 130.0, Normalize(dir)), 0, &photon_map, gather_photon_radius, gahter_max_photon_num);
				}
			}
		}
	}
	
	// .hdr�t�H�[�}�b�g�ŏo��
	save_hdr_file(std::string("image.hdr"), image, width, height);
}
